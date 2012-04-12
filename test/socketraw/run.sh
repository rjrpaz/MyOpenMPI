#!/bin/sh

HOST_REMOTO=10.1.255.252
DATE=`/bin/date +%s`

echo "Arranco server socket raw en host remoto"
ssh $HOST_REMOTO /home/robertorp/socketraw/server &
echo ""

sleep 1

echo "Arranco cliente local"
/home/robertorp/socketraw/client > socketraw.$DATE
echo ""

echo "Bajo server socket raw en host remoto"
ssh $HOST_REMOTO killall -SIGINT server

echo "Finalizado"
echo ""
