FROM mafintosh/base
ADD . /src
RUN cd /src; make install
ENTRYPOINT ["spawn", "tcp-echo-server"]
