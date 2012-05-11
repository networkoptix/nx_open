apt-get install -y libmp3lame-dev libssl-dev libx264-dev libxerces-c-dev libprotobuf-dev libqt4-dev libqjson-dev build-essential python-pip 

pushd appserver
python ./scripts/install_others.py
popd


