from os import system

def allLanguages():
    system('robot -d outputs -e not-ready -V getvars.py:en_US --timestampoutputs test-cases')
    system('robot -d outputs -e not-ready -V getvars.py:en_GB --timestampoutputs test-cases')
    system('robot -d outputs -e not-ready -V getvars.py:ru_RU --timestampoutputs test-cases')
    system('robot -d outputs -e not-ready -V getvars.py:fr_FR --timestampoutputs test-cases')
    system('robot -d outputs -e not-ready -V getvars.py:de_DE --timestampoutputs test-cases')
    system('robot -d outputs -e not-ready -V getvars.py:es_ES --timestampoutputs test-cases')
    system('robot -d outputs -e not-ready -V getvars.py:hu_HU --timestampoutputs test-cases')
    system('robot -d outputs -e not-ready -V getvars.py:zh_CN --timestampoutputs test-cases')
    system('robot -d outputs -e not-ready -V getvars.py:zh_TW --timestampoutputs test-cases')
    system('robot -d outputs -e not-ready -V getvars.py:ja_JP --timestampoutputs test-cases')
    system('robot -d outputs -e not-ready -V getvars.py:ko_KR --timestampoutputs test-cases')
    system('robot -d outputs -e not-ready -V getvars.py:tr_TR --timestampoutputs test-cases')
    system('robot -d outputs -e not-ready -V getvars.py:th_TH --timestampoutputs test-cases')
    system('robot -d outputs -e not-ready -V getvars.py:nl_NL --timestampoutputs test-cases')
    system('robot -d outputs -e not-ready -V getvars.py:he_IL --timestampoutputs test-cases')
    system('robot -d outputs -e not-ready -V getvars.py:pl_PL --timestampoutputs test-cases')
    system('robot -d outputs -e not-ready -V getvars.py:vi_VN --timestampoutputs test-cases')

if __name__ == "__main__":
    allLanguages()