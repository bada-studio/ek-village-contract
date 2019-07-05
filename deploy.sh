if [ "$1" = "real" ]
then
  url=https://rpc.eosys.io:443
  contract=eosknightsvg
elif [ "$1" = "beta" ]
then
  url=https://api-kylin.eoslaomao.com
  contract=eosknightsvg
elif [ "$1" = "local" ]
then
  url=http://127.0.0.1:8888
  contract=eosknightsvg
else
  echo "need phase"
  exit 1
fi

cleos -u $url set contract $contract village_contract -p $contract
