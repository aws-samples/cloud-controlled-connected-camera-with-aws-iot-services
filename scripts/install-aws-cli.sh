#!/bin/bash

if ! type "aws" > /dev/null; then
  sudo apt-get install python3-pip -y
  echo "installing the awscli"
  sudo pip3 install --upgrade awscli
fi

aws --version
