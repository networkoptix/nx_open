VERSION=$(python -c 'from version import VERSION; print(VERSION)')

function build()
{
    [ -d env ] || virtualenv -p python3 env
    source env/bin/activate
    pip install -r requirements.txt
    pip install twine
    python setup.py bdist_wheel
}

function publish()
{
    twine upload dist/monitoring_simple-${VERSION}-py3-none-any.whl -r la.hdw.mx
}

numargs=$#
for ((n=1;n <= numargs; n++))
do
	func=$1; shift
	$func
done
