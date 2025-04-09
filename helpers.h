#include <arpa/inet.h>  // htons(), ntohs()
#include <netdb.h>      // gethostbyname(), struct hostent
#include <netinet/in.h> // struct sockaddr_in
#include <stdio.h>      // perror(), fprintf()
#include <string.h>     // memcpy()
#include <sys/socket.h> // getsockname()
#include <unistd.h>     // stderr

int make_server_sockaddr(struct sockaddr_in *addr, int port) {
  addr->sin_family = AF_INET;
  addr->sin_addr.s_addr = INADDR_ANY;
  addr->sin_port = htons(port);
  return 0;
}

int make_client_sockaddr(struct sockaddr_in *addr, const char *hostname, int port) {
  addr->sin_family = AF_INET;
  struct hostent *host = gethostbyname(hostname);
  if (host == NULL) {
    fprintf(stderr, "%s: unknown host\n", hostname);
    return -1;
  }
  memcpy(&addr->sin_addr, host->h_addr, host->h_length);
  addr->sin_port = htons(port);
  return 0;
}

int get_port_number(int sockfd) {
  struct sockaddr_in addr;
  socklen_t length = sizeof(addr);
  if (getsockname(sockfd, (struct sockaddr *) &addr, &length) == -1) {
    perror("Error getting port of socket");
    return -1;
  }
  return ntohs(addr.sin_port);
}

void send_back(char* msg, int connectionfd, size_t message_len){
  size_t bytes_sent = 0;
  do {
    bytes_sent += send(connectionfd, msg + bytes_sent, 
    message_len - bytes_sent, MSG_NOSIGNAL);
  } while (bytes_sent < message_len);   
}

std::vector<std::string> get_dirs(std::string pathname){
  std::vector<std::string> dirs;
  std::string dir;
  std::istringstream path(pathname);
  getline(path, dir, '/');
  while (getline(path, dir, '/')) {
      dirs.push_back(dir);
  }
  return dirs;
}

int if_exists(fs_inode current, std::string dirname, std::string username){
  for (int i = 0; i < current.size; i++){
    if (current.owner[0] != 0 && current.owner != username){
      return 0;
    }
    fs_direntry fsd[FS_DIRENTRIES];
    disk_readblock(current.blocks[i], &fsd);
    for (auto &f: fsd){
      if (f.inode_block != 0 && f.name == dirname){
        return f.inode_block;
      }
    }
  }
  return 0;
  // 0 = not exists
  // blocknumber = exists
}

int if_exists_file(fs_inode current, std::string dirname, fs_direntry (&entries)[FS_DIRENTRIES]){ //
  int blk = 0;
  int i = 0;
  int flag = 0;
  for (; i < current.size; i++){
    fs_direntry fsd[FS_DIRENTRIES];
    disk_readblock(current.blocks[i], fsd);
    for (auto &f: fsd){
      if (f.inode_block != 0 && f.name == dirname){
        return -1;
      }
      if (f.inode_block == 0 && flag == 0){
        blk = current.blocks[i];
        memcpy(entries, fsd, sizeof(fsd));
        flag = 1;
      }
    }
  }
  if (blk == 0 && i == FS_MAXFILEBLOCKS){
    return -1;
  }
  return blk;
  // -1 = current is full or file exists
  // 0 = all the blocks contain 8 used entries
  // o.w. = the block that contains an unused entry
}

int if_exists_file_delete(fs_inode current, std::string dirname, fs_direntry (&entries)[FS_DIRENTRIES]){ //
  int blk = 0;
  int i = 0;
  for (; i < current.size; i++){
    fs_direntry fsd[FS_DIRENTRIES];
    disk_readblock(current.blocks[i], fsd);
    for (auto &f: fsd){
      if (f.inode_block != 0 && f.name == dirname){
        memcpy(entries, fsd, sizeof(fsd));
        return current.blocks[i];
      }
    }
  }
  return 0;
  // 0 = not exist
  // o.w. = the block that contains the direntry of deleted file
}

int unused_block(std::vector<int> disk){
  for (int i = 1;i < disk.size();i++){
    if (disk[i] == 0){
      return i;
    }
  }
  return 0;
}
// lock?
int unused_entries(fs_direntry* entries){
  for (int i = 0;i < FS_DIRENTRIES; i++){
    if (entries[i].inode_block == 0){
      return i;
    }
  }
}

int anyused_entries(fs_direntry* entries){
  for (int i = 0;i < FS_DIRENTRIES; i++){
    if (entries[i].inode_block != 0){
      return i;
    }
  }
  return FS_DIRENTRIES;
}

void erase_array(uint32_t (&blocks)[FS_MAXFILEBLOCKS], uint32_t block, int array_size){
  int i = 0;
  for (; i < array_size; i++){
    if (blocks[i] == block){
      break;
    }
  }
  for (int j = i; j < array_size - 1; j++){
    blocks[j] = blocks[j + 1];
  }
}

void disk_initialize(std::vector<int> &disk_used, int block){
  fs_inode cur;
  disk_used[block] = 1;
  disk_readblock(block, &cur);
  for (int i = 0;i < cur.size; i++){
    disk_used[cur.blocks[i]] = 1;
    if (cur.type == 'd'){
      fs_direntry dirs[FS_DIRENTRIES];
      disk_readblock(cur.blocks[i], dirs);
      for (auto &dir: dirs){
        if (dir.inode_block != 0){
          disk_initialize(disk_used, dir.inode_block);
        }
      }
    }
  }
  return;
}

class hoh_lock{
public:
  hoh_lock();
  ~hoh_lock();

  void swap(int a, int write_flag);

private:
  boost::shared_mutex* mutex;
  int cur_write_flag;
  int cur_block = -1;
};