FROM debian:jessie

ENV DEBIAN_FRONTEND noninteractive

RUN apt-get update && \
    apt-get install -y ssh python-setuptools libffi6 libyaml-0.2 ca-certificates && \
    easy_install pip

RUN pip install cryptography ansible && \
    ansible-galaxy install dj-wasabi.telegraf rossmcdonald.influxdb ansiblebit.grafana && \
    rm -rf /var/lib/apt/lists/*
    
COPY docker/merge_config.py /usr/local/bin/
COPY machine.py /machine.py
COPY playbooks /playbooks
COPY conf/ansible.cfg /etc/ansible/ansible.cfg
COPY docker/entrypoint.sh /entrypoint.sh

ENTRYPOINT ["/entrypoint.sh"]
CMD ["/playbooks/bootstrap.yml"]
