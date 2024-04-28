FROM ubuntu:22.04
RUN export noninteractive; apt update; apt install -y wget libreadline8 libusb-0.1-4 tmux make libmpc3
RUN wget https://github.com/pspdev/pspdev/releases/download/latest/pspdev-ubuntu-latest.tar.gz -O - | gzip -d | tar -C /usr/local -x
RUN echo 'export PATH="/usr/local/pspdev/bin:$PATH"' > /etc/profile.d/pspsdk.sh
RUN echo 'export LD_LIBRARY_PATH="/usr/local/pspsdk/lib:$LD_LIBRARY_PATH"' >> /etc/profile.d/pspsdk.sh
ENTRYPOINT ["/bin/bash", "-l"]
