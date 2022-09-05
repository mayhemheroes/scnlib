FROM fuzzers/libfuzzer:12.0

RUN apt-get update
RUN apt install -y build-essential wget clang cmake git xz-utils automake autotools-dev  libtool zlib1g zlib1g-dev libssl-dev curl
RUN git clone https://github.com/eliaskosunen/scnlib.git --recursive
WORKDIR /scnlib
RUN cmake -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++ .
RUN make
RUN make install
COPY fuzzers/fuzz.cpp .
RUN clang++ -I/usr/local/include -fsanitize=fuzzer,address fuzz.cpp -o /fuzz /usr/local/lib/libscn.a


ENTRYPOINT []
CMD ["/fuzz"]
