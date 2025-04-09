#!/bin/bash

# This script tags the state of the current branch and optionally pushes
# tags to the remote.  It also configures the local git repo to include tags
# during git push.

date=`TZ=America/Detroit date +%Y.%m.%d_%H.%M.%S`

# Make sure I'm in a git repository
out=`git rev-parse > /dev/null 2>&1`
if [[ $? -ne 0 ]]; then
    echo "Error: Not in a git repository.  Please run autotag.sh from a git repository."
    exit 1
fi

# Make sure there is at least one C++ source file in this git repository
top=`git rev-parse --show-toplevel`
out=`cd $top; git ls-files | grep -E '(cc|cpp)$' > /dev/null 2>&1`

if [[ $? -ne 0 ]]; then
    echo "Error: No C++ source files in repository.  Please add your source files to the repository."
    exit 1
fi

# Make sure this git repository has an initial commit
out=`git log > /dev/null 2>&1`
if [[ $? -ne 0 ]]; then
    echo "Error: No commits in repository.  Please create an initial commit."
    exit 1
fi

# Ignore signals, so users don't stop in an intermediate state
trap "" SIGINT SIGQUIT SIGTSTP

# Create first temporary commit
git commit -m compile-${date}-tmp1 --allow-empty > /dev/null
if [[ $? -eq 0 ]]; then

    # Create second temporary commit
    git commit -am compile-${date}-tmp2 --allow-empty > /dev/null

    if [[ $? -eq 0 ]]; then
	# create tag for the second temporary commit (fail silently if tag
	# already exists)
	git tag -a compile-${date} -m "" >& /dev/null

	# Undo second temporary commit.  Use --mixed to unstage the files
	# that were staged because of the -a argument to git commit
	git reset --mixed HEAD~ > /dev/null

	# Push tag (if requested on the command line).  Allow this to be killed
	# (without killing autotag.sh).
	if [[ "$1" = "push" ]]; then
	    (trap - SIGINT SIGQUIT SIGTSTP; git push --tags)
	fi
    fi

    # Undo first temporary commit.  Use --soft to preserve the files that
    # that were already staged before running autotag.sh
    git reset --soft HEAD~ > /dev/null

fi

# Configure git to push tags (in addition to the current branch)
remote=`git remote | head -1`
config=`git config --get-all "remote.$remote.push"`
if [[ ! $config =~ refs/tags/\* ]]; then
    git config --add "remote.$remote.push" refs/tags/*
fi
if [[ ! $config =~ HEAD ]]; then
    git config --add "remote.$remote.push" HEAD
fi
