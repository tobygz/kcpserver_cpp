rm -rf ./log/*
nohup ./start_net.sh >./net.log&
nohup ./start_game.sh >./game.log&
