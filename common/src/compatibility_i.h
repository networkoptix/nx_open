
struct QnCompatibilityItem
{
    QnCompatibilityItem() {}

    QnCompatibilityItem(QString v1, QString c1, QString v2)
        : ver1(v1), comp1(c1), ver2(v2)
    {
    }

    bool operator==(const QnCompatibilityItem& other) const
    {
        return ver1 == other.ver1 && comp1 == other.comp1 && ver2 == other.ver2;
    }

    QString ver1;
    QString comp1;
    QString ver2;
};

//QN_FUSION_DEFINE_FUNCTIONS(QnCompatibilityItem, (binary), (ver1)(comp1)(ver2))
QN_FUSION_DECLARE_FUNCTIONS(QnCompatibilityItem, (binary))
