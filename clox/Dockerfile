FROM debian
WORKDIR /clox
RUN apt update && apt install -y gcc gdb make valgrind
COPY . .
RUN make all
CMD ["tail", "-f", "/dev/null"]
