cd "$( dirname "${BASH_SOURCE[0]}" )"

VERSION=$(python -c 'from version import VERSION; print(VERSION)')

function version()
{
    echo $VERSION
}

function build()
{
    [ -d env ] || virtualenv -p python3 env
    source env/bin/activate

    rm -rf dist
    pip install -r requirements.txt
    python setup.py bdist_wheel
}

function publish()
{
    [ -d env ] || virtualenv -p python3 env
    source env/bin/activate

    pip install twine
    twine upload dist/monitoring_simple-${VERSION}-py3-none-any.whl -r la.hdw.mx
}

numargs=$#
for ((n=1;n <= numargs; n++))
do
	func=$1; shift
	$func
done
