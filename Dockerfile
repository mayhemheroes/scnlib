FROM fuzzers/libfuzzer:12.0 as builder

RUN apt-get update
RUN apt install -y build-essential wget clang cmake git xz-utils automake autotools-dev  libtool zlib1g zlib1g-dev libssl-dev curl
ADD . /scnlib
WORKDIR /
WORKDIR /scnlib
RUN git submodule init
RUN git submodule update
RUN cmake -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++ .
RUN make
RUN make install
COPY fuzzers/fuzz_scan.cpp .
RUN clang++ -I/usr/local/include -fsanitize=fuzzer,address fuzz_scan.cpp -o fuzz_scan /usr/local/lib/libscn.a

FROM fuzzers/libfuzzer:12.0
COPY --from=builder /scnlib/fuzz_scan /
COPY --from=builder /usr/local/lib/libscn.a /usr/local/lib/

ENTRYPOINT []
CMD ["/fuzz_scan"]
