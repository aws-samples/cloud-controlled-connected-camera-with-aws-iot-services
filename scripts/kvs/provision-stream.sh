#!/bin/bash
: '
Warning: This is expected be called from easy-install.sh in top level folder of the project
Path to the policy-document has been adjusted based on that.
i.e. --policy-document 'file://scripts/iot/iam-permission-document.json'
'

KVS_IOT_DIR=./script-output/iot

if [[ -z $STREAM_NAME ]]; then
  echo 'STREAM_NAME must be set'
  exit 1
fi

echo "Using $STREAM_NAME as Kinesis Video Stream Name"
echo "$STREAM_NAME" > $KVS_IOT_DIR/steam-name

# create stream
if aws kinesisvideo describe-stream --stream-name $STREAM_NAME 2>&1 | grep -q 'ResourceNotFoundException'; then
  echo "Kinesis Video Stream $STREAM_NAME does not exist; creating now..."
  aws kinesisvideo create-stream --stream-name $STREAM_NAME --data-retention-in-hours 2
fi
