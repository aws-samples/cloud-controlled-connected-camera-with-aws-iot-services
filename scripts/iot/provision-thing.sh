#!/bin/bash
: '
Warning: This is expected be called from easy-install.sh in top level folder of the project
Path to the policy-document has been adjusted based on that.
i.e. --policy-document 'file://scripts/iot/iam-permission-document.json'
'

THING_TYPE=kvs_example_camera
IAM_ROLE=KVSCameraCertificateBasedIAMRole
IAM_POLICY=KVSCameraIAMPolicy
IOT_ROLE_ALIAS=KvsCameraIoTRoleAlias
IOT_ROLE_ALIAS_POLICY=KvsCameraIoTRoleAliasPolicy
IOT_DEVICE_POLICY=KvsCameraIoTDevicePolicy

KVS_IOT_DIR=./script-output/iot
CMD_RESULTS_DIR=$KVS_IOT_DIR/cmd-responses
CERTS_DIR=$KVS_IOT_DIR/certs
POLICY_DOCUMENT_DIR=./scripts/iot

if [[ -z $THING_NAME ]]; then
  echo 'THING_NAME must be set'
  exit 1
fi

mkdir -p $CMD_RESULTS_DIR
mkdir -p $CERTS_DIR

echo "Using $THING_NAME as IoT Thing Name"
echo "$THING_NAME" > $KVS_IOT_DIR/thing-name
echo "$IOT_ROLE_ALIAS" > $KVS_IOT_DIR/role-alias

# create thing type and thing
if aws iot describe-thing-type --thing-type-name $THING_TYPE 2>&1 | grep -q 'ResourceNotFoundException'; then
  echo "Thing type $THING_TYPE does not exist; creating now..."
  aws iot create-thing-type --thing-type-name $THING_TYPE > $CMD_RESULTS_DIR/iot-thing-type.json
fi

if aws iot describe-thing --thing-name $THING_NAME 2>&1 | grep -q 'ResourceNotFoundException'; then
  echo "Thing $THING_NAME does not exist; creating now..."
  aws iot create-thing --thing-name $THING_NAME --thing-type-name $THING_TYPE > $CMD_RESULTS_DIR/iot-thing.json
fi

# create AWS_IOT_ROLE_ALIAS (IAM Role, IAM Policy, IoT Role Alias, IoT Policy)
# see https://docs.aws.amazon.com/kinesisvideostreams/latest/dg/how-iot.html

if aws iam get-role --role-name $IAM_ROLE 2>&1 | grep -q 'NoSuchEntity'; then
  echo "IAM Role $IAM_ROLE does not exist; creating now..."
  aws iam create-role --role-name $IAM_ROLE \
    --assume-role-policy-document "file://$POLICY_DOCUMENT_DIR/iam-policy-document.json" > $CMD_RESULTS_DIR/iam-role.json
else
  aws iam get-role --role-name $IAM_ROLE > $CMD_RESULTS_DIR/iam-role.json
fi

if aws iam get-role-policy --role-name $IAM_ROLE --policy-name $IAM_POLICY 2>&1 | grep -q 'NoSuchEntity'; then
  echo "IAM Role Policy $IAM_POLICY does not exist; creating now..."
  aws iam put-role-policy --role-name $IAM_ROLE \
    --policy-name $IAM_POLICY --policy-document "file://$POLICY_DOCUMENT_DIR/iam-permission-document.json"
fi

if aws iot describe-role-alias --role-alias $IOT_ROLE_ALIAS 2>&1 | grep -q 'ResourceNotFoundException'; then
  echo "IoT Role Alias $IOT_ROLE_ALIAS does not exist; creating now..."
  aws iot create-role-alias --role-alias $IOT_ROLE_ALIAS \
    --role-arn $(jq --raw-output '.Role.Arn' $CMD_RESULTS_DIR/iam-role.json) \
    --credential-duration-seconds 3600 > $CMD_RESULTS_DIR/iot-role-alias.json
else
  aws iot describe-role-alias --role-alias $IOT_ROLE_ALIAS  > $CMD_RESULTS_DIR/iot-role-alias.json
fi

if aws iot get-policy --policy-name $IOT_ROLE_ALIAS_POLICY 2>&1 | grep -q 'ResourceNotFoundException'; then
  echo "IoT Policy $IOT_ROLE_ALIAS_POLICY does not exist; creating now..."
# joson node for roleAliasArn is different based on the executed command
# create-role-alias -> .roleAliasArn
# describe-role-alias -> .roleAliasDescription.roleAliasArn
cat > $POLICY_DOCUMENT_DIR/iot-kvs-policy-document.json <<EOF
{
	"Version": "2012-10-17",
	"Statement": [
		{
			"Effect": "Allow",
			"Action": [
				"iot:Connect"
			],
	    "Resource":"$(jq --raw-output 'try(.roleAliasArn) // .roleAliasDescription.roleAliasArn' $CMD_RESULTS_DIR/iot-role-alias.json)"
		},
		{
			"Effect": "Allow",
			"Action": [
				"iot:AssumeRoleWithCertificate"
			],
			"Resource":"$(jq --raw-output 'try(.roleAliasArn) // .roleAliasDescription.roleAliasArn' $CMD_RESULTS_DIR/iot-role-alias.json)"
		}
	]
}
EOF

  aws iot create-policy --policy-name $IOT_ROLE_ALIAS_POLICY \
    --policy-document "file://$POLICY_DOCUMENT_DIR/iot-kvs-policy-document.json"  > $CMD_RESULTS_DIR/iot-kvs-policy-document.json
fi

if aws iot get-policy --policy-name $IOT_DEVICE_POLICY 2>&1 | grep -q 'ResourceNotFoundException'; then
  echo "IoT Policy $IOT_DEVICE_POLICY does not exist; creating now..."
# The thing name is obtained from the client ID in the MQTT Connect message sent when a thing connects to AWS IoT Core.
cat > $POLICY_DOCUMENT_DIR/iot-device-policy-document.json <<EOF
{
  "Version": "2012-10-17",
  "Statement": [
    {
      "Effect": "Allow",
      "Action": [
        "iot:Publish"
      ],
      "Resource": [
        "$(jq --raw-output 'try(.roleAliasArn) // .roleAliasDescription.roleAliasArn' $CMD_RESULTS_DIR/iot-role-alias.json | sed 's/\:rolealias\/KvsCameraIoTRoleAlias//g'):topic/\$aws/things/\${iot:Connection.Thing.ThingName}/shadow/*"
      ]
    },
    {
      "Effect": "Allow",
      "Action": [
        "iot:Receive"
      ],
      "Resource": [
        "$(jq --raw-output 'try(.roleAliasArn) // .roleAliasDescription.roleAliasArn' $CMD_RESULTS_DIR/iot-role-alias.json | sed 's/\:rolealias\/KvsCameraIoTRoleAlias//g'):topic/\$aws/things/\${iot:Connection.Thing.ThingName}/shadow/*"
      ]
    },
    {
      "Effect": "Allow",
      "Action": [
        "iot:Subscribe"
      ],
      "Resource": [
        "$(jq --raw-output 'try(.roleAliasArn) // .roleAliasDescription.roleAliasArn' $CMD_RESULTS_DIR/iot-role-alias.json | sed 's/\:rolealias\/KvsCameraIoTRoleAlias//g'):topicfilter/\$aws/things/\${iot:Connection.Thing.ThingName}/shadow/*"
      ]
    },
    {
      "Effect": "Allow",
      "Action": "iot:Connect",
      "Resource": "$(jq --raw-output 'try(.roleAliasArn) // .roleAliasDescription.roleAliasArn' $CMD_RESULTS_DIR/iot-role-alias.json | sed 's/\:rolealias\/KvsCameraIoTRoleAlias//g'):client/\${iot:Connection.Thing.ThingName}"
    }
  ]
}
EOF

  aws iot create-policy --policy-name $IOT_DEVICE_POLICY \
    --policy-document "file://$POLICY_DOCUMENT_DIR/iot-device-policy-document.json" > $CMD_RESULTS_DIR/iot-device-policy-document.json
fi

# create keys and certificate
# certs to be saved in:
# $CERTS_DIR/device.cert.pem
# $CERTS_DIR/device.private.key
# $CERTS_DIR/root-CA.pem

# https://docs.aws.amazon.com/iot/latest/developerguide/iot-dc-prepare-device-test.html#iot-dc-prepare-device-test-step3
# extension should be pem.
# if the extension is crt, you will get "Unable to create Iot Credential provider. Error status: 0x15000020" error.
if [ ! -f "$CERTS_DIR/root-CA.pem" ]; then
  curl --silent 'https://www.amazontrust.com/repository/AmazonRootCA1.pem' \
    --output $CERTS_DIR/root-CA.pem
fi

if [ ! -f "$CERTS_DIR/device.cert.pem" ]; then
  echo "IoT Certificate does not exist; creating and attaching policies now..."
  aws iot create-keys-and-certificate --set-as-active \
    --certificate-pem-outfile $CERTS_DIR/device.cert.pem \
    --public-key-outfile $CERTS_DIR/device.public.key \
    --private-key-outfile $CERTS_DIR/device.private.key > $CMD_RESULTS_DIR/keys-and-certificate.json

  aws iot attach-policy --policy-name $IOT_ROLE_ALIAS_POLICY \
    --target $(jq --raw-output '.certificateArn' $CMD_RESULTS_DIR/keys-and-certificate.json)

  aws iot attach-policy --policy-name $IOT_DEVICE_POLICY \
    --target $(jq --raw-output '.certificateArn' $CMD_RESULTS_DIR/keys-and-certificate.json)

  aws iot attach-thing-principal --thing-name $THING_NAME \
    --principal $(jq --raw-output '.certificateArn' $CMD_RESULTS_DIR/keys-and-certificate.json)
fi

# get credential provider endpoint
if [ ! -f "credential-provider-endpoint" ]; then
  aws iot describe-endpoint --endpoint-type iot:CredentialProvider \
    --output text > $KVS_IOT_DIR/credential-provider-endpoint
fi

# get iot core data ATS endpoint
if [ ! -f "iot-core-endpoint" ]; then
  aws iot describe-endpoint --endpoint-type iot:Data-ATS \
    --output text > $KVS_IOT_DIR/iot-core-endpoint
fi
