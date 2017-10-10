FROM debian:jessie

ENV DEBIAN_FRONTEND noninteractive

RUN apt-get update && \
    apt-get install -y ssh python-setuptools libffi6 libyaml-0.2 ca-certificates && \
    easy_install pip

RUN pip install cryptography ansible && \
    ansible-galaxy install dj-wasabi.telegraf && \    
    rm -rf /var/lib/apt/lists/*
    
ADD ./machine.py /machine.py
ADD ./playbooks /playbooks
ADD ./conf/ansible.cfg /etc/ansible/ansible.cfg
ADD ./entrypoint.sh /entrypoint.sh
ENTRYPOINT ["/entrypoint.sh"]
CMD ["/playbooks/bootstrap.yml"]
