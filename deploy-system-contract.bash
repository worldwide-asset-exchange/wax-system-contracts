#!/bin/bash

# eosc and cleos must be in your PATH to run this script
# get eosc here: https://github.com/eoscanada/eosc/releases
# get cleos here: https://github.com/worldwide-asset-exchange/wax-blockchain

PROPOSER_ACCOUNT=$1

if [ -z $PROPOSER_ACCOUNT ]
then
  echo "Must pass the proposer account as the one and only argument to this script Ex. 'deploy-system-contract.bash 4tioi.waa'"
  exit 1 || return 1
fi

read -p "Deploying system contract. Are you sure? " -n 1 -r
echo    # (optional) move to a new line
if [[ ! $REPLY =~ ^[Yy]$ ]]
then
    [[ "$0" = "$BASH_SOURCE" ]] && exit 1 || return 1 # handle exits from shell or function but don't exit interactive shell
fi

production_url=https://chain.wax.io

chain_url=$production_url

CLEOS="cleos -u $chain_url"
EOSC="eosc -u $chain_url"
TEMP_VAULT_FILE=temp-eosc-vault.json
DEPLOYMENT_FILE=system-contracts-deploy.json
PROPOSAL_NAME=syscontract

function cleanup {
  rm $TEMP_VAULT_FILE
}
trap cleanup EXIT

echo "Setup your temporary key vault for the proposer account ${PROPOSER_ACCOUNT}..."
$EOSC vault create --import --vault-file $TEMP_VAULT_FILE

make clean # if this fails, do `sudo make clean`
rm $DEPLOYMENT_FILE
make dev-docker-all
$CLEOS set contract eosio ./build/contracts/eosio.system/ -x 604800 -s -d --json > $DEPLOYMENT_FILE
$EOSC multisig cancel $PROPOSER_ACCOUNT $PROPOSAL_NAME $PROPOSER_ACCOUNT --vault-file $TEMP_VAULT_FILE || echo "Proposal did not already exist - excellent"
$EOSC multisig propose $PROPOSER_ACCOUNT $PROPOSAL_NAME $DEPLOYMENT_FILE --request admin.wax --with-subaccounts --vault-file $TEMP_VAULT_FILE
echo "use this command to review the multisig proposal: '$EOSC multisig review $PROPOSER_ACCOUNT $PROPOSAL_NAME'"
echo "use this command to execute the multisig proposal: '$EOSC multisig exec $PROPOSER_ACCOUNT $PROPOSAL_NAME $PROPOSER_ACCOUNT' (You will probably need to create a vault for your proposer account 'eosc vault create --import')"
echo "use this command to cancel the multisig proposal: '$EOSC multisig cancel $PROPOSER_ACCOUNT $PROPOSAL_NAME $PROPOSER_ACCOUNT' (You will probably need to create a vault for your proposer account 'eosc vault create --import')"