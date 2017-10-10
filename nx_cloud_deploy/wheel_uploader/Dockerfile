FROM python:2.7
MAINTAINER Ivan Vigasin <ivigasin@gmail.com>

COPY pypirc /root/.pypirc

WORKDIR /app
RUN mkdir wheelhouse
RUN pip install twine wheel

VOLUME /app

CMD ["sh", "-c", "pip wheel --no-cache-dir -r /dev/stdin -w wheelhouse ; twine upload -r la.hdw.mx wheelhouse/*"]
