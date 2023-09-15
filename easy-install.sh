#!/bin/bash

if [[ -z $AWS_ACCESS_KEY_ID || -z $AWS_SECRET_ACCESS_KEY || -z $AWS_DEFAULT_REGION ]]; then
  echo 'AWS_ACCESS_KEY_ID, AWS_SECRET_ACCESS_KEY, and AWS_DEFAULT_REGION must be set'
  exit 1
fi

export THING_NAME=$1

if [[ -z $THING_NAME ]]; then
  # prompt for thing name
  echo -n "Enter a Name for your IoT Thing: "
  read THING_NAME
fi 

# ust thing name as a stream name
export STREAM_NAME=$THING_NAME

echo "installing build dependencies"
sudo apt-get install -y jq zip pkg-config cmake

# ./scripts/install-aws-cli.sh

# ./scripts/iot/provision-thing.sh
# ./scripts/kvs/provision-stream.sh

# generate run-c3-camera.sh using outputs from previous setps
echo "generating run-c3-camera.sh under $(pwd)"
cat > ./run-c3-camera.sh <<EOF
#!/bin/bash

export DEMO_TYPE=\$1

if [[ -z \$DEMO_TYPE ]]; then
  # prompt for thing name
  echo -n "Enter the DEMO TYPE( producer, webrtc ) to use: "
  read DEMO_TYPE
fi 

cd $(pwd)/build
sudo ./c3-camera-\$DEMO_TYPE --thing_name `cat $(pwd)/script-output/iot/thing-name` \\
--client_id `cat $(pwd)/script-output/iot/thing-name` \\
--shadow_property pan,tilt \\
--endpoint `cat $(pwd)/script-output/iot/iot-core-endpoint` \\
--cert $(pwd)/script-output/iot/certs/device.cert.pem \\
--key $(pwd)/script-output/iot/certs/device.private.key \\
--ca_file $(pwd)/script-output/iot/certs/root-CA.pem \\
--credential_endpoint `cat $(pwd)/script-output/iot/credential-provider-endpoint` \\
--role_alias `cat $(pwd)/script-output/iot/role-alias` \\
--kvs_region $AWS_DEFAULT_REGION
EOF

sudo chmod 755 ./run-c3-camera.sh
