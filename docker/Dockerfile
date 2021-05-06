FROM ubuntu:20.04

LABEL maintainer="Adriano Lange"
LABEL email="alange0001@gmail.com"

RUN apt-get update

RUN apt-get install -y libgflags-dev libsnappy-dev zlib1g-dev libbz2-dev liblz4-dev libzstd-dev openjdk-14-jre libaio1

RUN apt-get clean

RUN rm -fr /opt
COPY opt /opt

ENV PATH /usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin:/opt/rocksdb_test/bin:/opt/YCSB/bin:/opt/rocksdb/bin
ENV LD_LIBRARY_PATH /opt/rocksdb/lib
ENV YCSB_PATH /opt/YCSB

ENV HOME /root
WORKDIR /root
CMD ["bash"]
