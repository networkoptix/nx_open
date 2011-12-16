for i in common server client appserver
do
  pushd $i
  python convert.py
  popd
done

for i in common server client
do
  pushd $i/build
  make -f Makefile.debug -j9
  popd
done
