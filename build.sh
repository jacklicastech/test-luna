set -e

manuf=$1
target=$2
name=$manuf-$target
artifacts_dir=tmp/artifacts/$manuf/$target

case $manuf in
  castles)
    artifacts_names='libsodium.cap libzmq.cap libczmq.cap liburiparser.cap
                     liblua.cap application.cap resources.cap plugins.cap
                     bindings.cap luna.tar.gz'
    ;;
  *)
    echo 'manufacturer not recognized'
    exit 1
    ;;
esac

if [ "x$target" = 'x' ]
then
  echo 'target required'
  exit 1
fi

if [ -d $artifacts_dir ]
then
  rm -rf $artifacts_dir
fi

if [ -d ./luna ]
then
  echo './luna exists'
  if [ -d ./luna/.git ]
  then
    cd luna
    git pull origin master
    cd ..
  fi
  tar czf luna.tgz luna
else
  if [ -f ./luna.tgz ]
  then
    echo './luna.tgz exists, not cloning luna'
  else
    git submodule add git@github.com:appilee/luna luna
    tar czf luna.tgz luna
  fi
fi
mv luna.tgz $manuf/$target/luna.tgz

# if [ ! -f $manuf/base/toolchain.tgz ]
# then
#   aws s3 cp s3://appilee/sdk/$manuf/toolchain.tgz $manuf/base/toolchain.tgz
# fi


if [ ! -f $manuf/base/capgen.tgz ]
then
  aws s3 cp s3://appilee/sdk/$manuf/capgen.tgz $manuf/base/capgen.tgz
fi
docker build -t $manuf-base $manuf/base


if [ ! -f $manuf/$target/sdk.tgz ]
then
  aws s3 cp s3://appilee/sdk/$manuf/$target.tgz $manuf/$target/sdk.tgz
fi

mkdir -p $artifacts_dir
docker build -t $name       $manuf/$target
rm $manuf/$target/luna.tgz
id=$(docker create $name)
for artifact_name in $artifacts_names
do
  if [ "x$artifact_name" != "xluna.tar.gz" ]
  then
    docker cp $id:/tmp/src/luna/dist/output/$artifact_name $artifacts_dir
  else
    docker cp $id:/tmp/src/luna/$artifact_name $artifacts_dir
  fi
done
docker rm -v $id
if [ "x$NOS3" = "x" ]
then
  aws s3 sync $artifacts_dir s3://appilee/luna/tip/$target
fi

cd luna
aws s3 cp s3://appilee/luna/tip/$target/index.json ./index.json
for artifact in $artifacts_names
do
  if [ "x$artifact" != "xluna.tar.gz" ]
  then
    rake update_index PREFIX=luna/tip/$target KEY=$artifact
  fi
done
aws s3 cp ./index.json s3://appilee/luna/tip/$target/index.json
