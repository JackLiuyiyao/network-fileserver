#include "fs_server.h"
#include <arpa/inet.h>  // htons()
#include <stdio.h>      // printf(), perror()
#include <stdlib.h>     // atoi()
#include <sys/socket.h> // socket(), bind(), listen(), accept(), send(), recv()
#include <unistd.h>     // close()
#include "helpers.h"
#include "fs_param.h"
#include <sstream>
#include <vector>
#include <boost/regex.hpp>

static const size_t MAX_REQ_SIZE = 158;
static const size_t MAX_MESSAGE_SIZE = 158 + 512;

std::vector<int> disk_used = {};
std::vector<boost::shared_mutex*> block_mutex;
boost::shared_mutex disk_mutex;

void sharelock(boost::shared_mutex* mx, int write){
  if (write){
    mx->lock();
  } else {
    mx->lock_shared();
  }
}

void shareunlock(boost::shared_mutex* mx, int write){
  if (write){
    mx->unlock();
  } else {
    mx->unlock_shared();
  }
}

hoh_lock::hoh_lock(){}

hoh_lock::~hoh_lock(){
  if (cur_block != - 1){
    shareunlock(mutex, cur_write_flag);
  }
  cur_block = -1;
}

void hoh_lock::swap(int a, int write_flag){
  auto tmp = mutex;
  mutex = block_mutex[a];
  sharelock(mutex, write_flag);
  if (cur_block != -1){ // -1 = does not hold lock
    shareunlock(tmp, cur_write_flag);
  }
  cur_write_flag = write_flag;
  cur_block = a;
}

int checkBlockNum(std::string blockNum){
  boost::regex pattern("^(?:[1-9]\\d*|0)$");
  if (boost::regex_search(blockNum, pattern) && stoi(blockNum) <= FS_MAXFILEBLOCKS - 1 && stoi(blockNum) >= 0) {
    return 0;
  } else {
    return -1;
  }
}

int checkUsername(std::string username, bool root = false){
  if (root == false){
    if (username.length() == 0 || username.length() > FS_MAXUSERNAME){
      return -1;
    }
  } else {
    if (username.length() > FS_MAXUSERNAME){
      return -1;
    }
  }
  return 0;
}

int checkPathname(std::string pathname){
  if (pathname.length() > FS_MAXPATHNAME || pathname.length() == 0 || pathname[0] != '/'){
    return -1;
  }
  if (pathname.length() > 1 && pathname[pathname.length() - 1] == '/'){
    return -1;
  }
  int len = 0;
  for (int i=1; i<pathname.length(); i++){
    if (len >= FS_MAXFILENAME){
      return -1;
    }
    if (pathname[i] != '/'){
      len++;
    } else {
      if (pathname[i - 1] == '/'){
        return -1;
      }
      len = 0;
    }
  }
  return 0;
}

int checkmsg(char* msg, int spacenum){
  int count = 0;
  while(*msg){
    if (static_cast<unsigned char>(*msg) == ' '){
      count++;
    }
    msg++;
  }
  if (count == spacenum){
    return 0;
  } else {
    return -1;
  }
}

int nulltm(char* msg){
  int i = 0;
  while(msg[i]){
    i++;
  }
  return i;
}

int compare_correct(std::string correct, std::string msg){
  if (correct.length() != msg.length()){
    return 0;
  }
  for (int i=0;i<correct.length();i++){
    if (correct[i] != msg[i]){
      return 0;
    }
  }
  return 1;
}

int handle_connection(int connectionfd) {
  char msg[MAX_MESSAGE_SIZE + 1];
  memset(msg, 0, sizeof(msg));

  ssize_t recvd = 0;
  ssize_t rval;
  do {
    rval = recv(connectionfd, msg + recvd, MAX_MESSAGE_SIZE - recvd, 0);
    if (rval == -1) {
      perror("Error reading stream message");
      return -1;
    }
    recvd += rval;
    if (nulltm(msg) >= MAX_REQ_SIZE){
      break;
    }
    if (nulltm(msg) < recvd){
      std::string msgstr(msg);
      std::istringstream request(msg);
      std::string reqtype, username, pathname;
      request >> reqtype >> username >> pathname;

      hoh_lock mx;

      if(reqtype == "FS_CREATE"){
        std::string type;
        request >> type;
        std::string correct;
        correct = reqtype + " " + username + " " + pathname + " " + type;
        if (!compare_correct(correct, msgstr)){
          break;
        }
        if (checkmsg(msg, 3) || checkPathname(pathname) || checkUsername(username, pathname.length()==1) || !(type=="f"||type == "d") || pathname.size() <= 1){
          break;
        }
        auto dirs = get_dirs(pathname);

        int inode = 0;
        fs_inode current;
        mx.swap(0, dirs.size() == 1); // write lock for creating file under root
        disk_readblock(0, &current);
        // create /dir/dir2/dir3/file
        // create /dir
        int flag = 0;
        for (int j = 0; j < dirs.size() - 1; j++){
          inode = if_exists(current, dirs[j], username);
          if (current.type != 'd' || !inode){
            flag = 1;
            break;
          }
          mx.swap(inode, j == dirs.size() - 2); // write lock for the last one
          disk_readblock(inode, &current);
        }
        if (flag) {break;}
        std::string filename = dirs[dirs.size() - 1];
        fs_direntry entries[FS_DIRENTRIES];
        if (current.owner[0] != 0 && current.owner != username){
          break;
        }
        int inodeblk = if_exists_file(current, filename, entries);
        if (inodeblk == -1){
          break;
        }
        fs_inode file;
        file.type = type[0];
        strcpy(file.owner, username.c_str());
        
        file.size = 0;

        boost::unique_lock<boost::shared_mutex> disk_unique(disk_mutex);
        int block = unused_block(disk_used);
        if (block == 0) {break;}
        disk_used[block] = 1;
        disk_unique.unlock();

        fs_direntry filedir;
        strcpy(filedir.name, filename.c_str());
        filedir.inode_block = block;

        if (inodeblk == 0){

          boost::unique_lock<boost::shared_mutex> disk_unique2(disk_mutex);
          int newblk = unused_block(disk_used);
          if (newblk == 0) {break;}
          disk_used[newblk] = 1;
          disk_unique2.unlock();

          current.blocks[current.size] = newblk;
          current.size++;
          fs_direntry newentry[FS_DIRENTRIES];

          // init direntries
          for (auto &d : newentry){
            d.inode_block = 0;
          }
          newentry[0] = filedir;

          disk_writeblock(block, &file);
          disk_writeblock(newblk, &newentry);
          disk_writeblock(inode, &current);

          // disk_used[newblk] = 1;
          // disk_unique2.unlock();
        } else {
          int entry = unused_entries(entries);
          entries[entry] = filedir;
          disk_writeblock(block, &file);
          disk_writeblock(inodeblk, entries);
        }
        mx.~hoh_lock();

        send_back(msg, connectionfd, nulltm(msg) + 1);
        break;
      }
      else if(reqtype == "FS_DELETE"){

        std::string correct;
        correct = reqtype + " " + username + " " + pathname;
        if (!compare_correct(correct, msgstr)){
          break;
        }

        if (pathname.length() == 1 || checkmsg(msg, 2) || checkPathname(pathname) || checkUsername(username, false)){
          break;
        }
        
        auto dirs = get_dirs(pathname);

        int current_block = 0;
        fs_inode current;
        // 1: delete /dir/dir2/dir3/dir4
        // 2: create /dir/dir2/dir3/dir4/file

        // 1: delete /dir/dir2/dir3/file
        // 2: write /dir/dir2/dir3/file

        // 1: delete /dir/dir2/dir3/file
        // 2: read /dir/dir2/dir3/file

        mx.swap(0, dirs.size() == 1);
        disk_readblock(0, &current);
        // check all the parent dirs exist
        int flag = 0;
        for (int j = 0; j < dirs.size() - 1; j++){
          current_block = if_exists(current, dirs[j], username);
          if (current.type != 'd' || !current_block){
            flag = 1;
            break;
          }
          mx.swap(current_block, j == dirs.size() - 2);
          disk_readblock(current_block, &current);
        }
        if (flag || (current.owner[0] != 0 && current.owner != username)) {break;}
        
        // find the direntry block that contains the file to delete
        fs_direntry entries[FS_DIRENTRIES];
        int delete_entry_block = if_exists_file_delete(current, dirs[dirs.size() - 1], entries);
        if (!delete_entry_block) {break;}

        // find the index of file direntry
        int delete_entry_num = 0;
        for (; delete_entry_num < FS_DIRENTRIES; delete_entry_num++){
          if (entries[delete_entry_num].inode_block != 0 && entries[delete_entry_num].name == dirs[dirs.size() - 1]){
            break;
          }
        }

        int delete_inode_block = entries[delete_entry_num].inode_block;
        fs_inode delete_inode;

        hoh_lock mx2;
        mx2.swap(delete_inode_block, 1);
        disk_readblock(delete_inode_block, &delete_inode);

        if (delete_inode.owner != username || (delete_inode.type == 'd' && delete_inode.size > 0)){
          break;
        }

        entries[delete_entry_num].inode_block = 0;
        if (anyused_entries(entries) == FS_DIRENTRIES){
          erase_array(current.blocks, delete_entry_block, current.size);
          current.size--;
          disk_writeblock(current_block, &current);

          disk_mutex.lock();
          disk_used[delete_entry_block] = 0;
          disk_mutex.unlock();
        } else {
          disk_writeblock(delete_entry_block, entries);
        }
        // 1: read /dir/file data:[3]
        // 2: delete /dir/file disk_used[3] = 0;
        // 3: create /aa 3

        // update direntries / parent inode
        disk_mutex.lock();
        for (int i=0; i<delete_inode.size; i++){
          disk_used[delete_inode.blocks[i]] = 0;
        }
        disk_used[delete_inode_block] = 0;
        disk_mutex.unlock();

        mx.~hoh_lock();
        mx2.~hoh_lock();

        send_back(msg, connectionfd, nulltm(msg) + 1);
        break;
      }
      else if(reqtype == "FS_READBLOCK"){
        std::string block;
        request >> block;

        std::string correct;
        correct = reqtype + " " + username + " " + pathname + " " + block;
        if (!compare_correct(correct, msgstr)){
          break;
        }

        if (checkmsg(msg, 3) || checkPathname(pathname) || checkUsername(username, false) || checkBlockNum(block) || pathname.length()==1){
          break;
        }
        auto dirs = get_dirs(pathname);

        int inode = 0;
        fs_inode current;
        mx.swap(0, 0);
        disk_readblock(0, &current);
        int flag = 0;
        for (int j = 0; j < dirs.size(); j++){
          inode = if_exists(current, dirs[j], username);
          if (current.type != 'd' || !inode){
            flag = 1;
            break;
          }
          mx.swap(inode, 0);
          disk_readblock(inode, &current);
        }
        if (flag) {break;}
        if (current.type != 'f' || current.owner != username || stoi(block) >= current.size){break;}
        char data[FS_BLOCKSIZE];
        disk_readblock(current.blocks[stoi(block)], data);
        mx.~hoh_lock();
        memcpy(msg + nulltm(msg) + 1, data, 512);
        send_back(msg, connectionfd, nulltm(msg) + 513);
        break;
      }
      else if(reqtype == "FS_WRITEBLOCK"){
        int start = nulltm(msg) + 1;
        if (recvd - start < 512){
          recv(connectionfd, msg + recvd, 512 - (recvd - start), MSG_WAITALL);
        }
        std::string block;
        request >> block;

        std::string correct;
        correct = reqtype + " " + username + " " + pathname + " " + block;
        if (!compare_correct(correct, msgstr)){
          break;
        }

        if (checkmsg(msg, 3) || checkPathname(pathname) || checkUsername(username, false) || checkBlockNum(block) || pathname.length()==1){
          break;
        }
        
        auto dirs = get_dirs(pathname);
        int inode = 0;
        fs_inode current;
        mx.swap(0, 0);
        disk_readblock(0, &current);
        int flag = 0;
        for (int j = 0; j < dirs.size(); j++){
          inode = if_exists(current, dirs[j], username);
          if (current.type != 'd' || !inode){
            flag = 1;
            break;
          }
          mx.swap(inode, j == dirs.size() - 1);
          disk_readblock(inode, &current);
        }
        if (flag) {break;}
        if (current.type != 'f' || current.owner != username || stoi(block) > current.size){break;}
        flag = 0;

        if (stoi(block) == current.size){
          if (current.size == FS_MAXFILEBLOCKS){
            break;
          }
          boost::unique_lock<boost::shared_mutex> disk_unique(disk_mutex);
          current.blocks[current.size] = unused_block(disk_used);
          if (current.blocks[current.size] == 0) {break;}
          disk_used[current.blocks[current.size]] = 1;
          disk_unique.unlock();
          current.size++;
          flag = 1;
        }
        // store data
        char data[FS_BLOCKSIZE];
        memcpy(data, msg + start, 512);
        disk_writeblock(current.blocks[stoi(block)], data);
        if (flag == 1){
          disk_writeblock(inode, &current);
        }
        mx.~hoh_lock();
        send_back(msg, connectionfd, nulltm(msg) + 1);
        break;
      }
      else{
        // return -1;
        break;
      }
    }
  } while (rval > 0);

  close(connectionfd);
  return 0;
}

int run_server(int port) {

  // (1) Create socket
  int sockfd = socket(AF_INET, SOCK_STREAM, IPPROTO_IP);
  if (sockfd == -1) {
    perror("Error opening stream socket");
    return -1;
  }

  // (2) Set the "reuse port" socket option
  int yesval = 1;
  if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yesval, sizeof(yesval)) == -1) {
    perror("Error setting socket options");
    return -1;
  }

  // (3) Create a sockaddr_in struct for the proper port and bind() to it.
  sockaddr_in addr{};       // initializes with zeroes
  addr.sin_family = AF_INET;
  addr.sin_addr.s_addr = htonl(INADDR_ANY);
  addr.sin_port = htons(port);

  // (3b) Bind to the port.
  bind(sockfd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr));

  // (3c) Detect which port was chosen.
  port = get_port_number(sockfd);
  
  // (4) Begin listening for incoming connections.
  if (listen(sockfd, 30) == -1) {
    perror("Error listening");
    return -1;
  }
  print_port(port);
  // (5) Serve incoming connections one by one forever.
  while (1) {
    int connectionfd = accept(sockfd, NULL, NULL);
    boost::thread t(handle_connection, connectionfd);
    t.detach();
  }
  return 0;
}

int main(int argc, const char **argv) {
  int port = 0;
  if (argc == 2){
    port = atoi(argv[1]);
  } 
  disk_used.resize(FS_DISKSIZE, 0);
  disk_used[0] = 1;
  disk_initialize(disk_used, 0);
  for (int i=0; i < FS_DISKSIZE; i++){
    boost::shared_mutex* a = new boost::shared_mutex;
    block_mutex.push_back(a);
  }
  if (run_server(port) == -1) {
    return 1;
  }
  return 0;
}