#!/bin/sh

HOST_REMOTO=10.1.255.252
DATE=`/bin/date +%s`

echo "Arranco server socket raw en host remoto"
ssh $HOST_REMOTO /tmp/server &
echo ""

sleep 1

echo "Arranco cliente local"
/tmp/client > socketraw.$DATE
echo ""

echo "Bajo server socket raw en host remoto"
ssh $HOST_REMOTO killall -SIGINT server

echo "Finalizado"
echo ""
