#ifndef GNUPGPREFERENCES_H
#define GNUPGPREFERENCES_H

#include <kcmodule.h>

#include <QVariantList>

class QCheckBox;
class QTableView;

class GnupgPreferences: public KCModule
{
    Q_OBJECT
public:
    explicit GnupgPreferences(QWidget *parent=0, const QVariantList &args = QVariantList() );
    virtual ~GnupgPreferences();
    virtual void save();
    virtual void load();
    virtual void defaults();

private slots:
  void addPair();
  void remPair();
    
private:
    QCheckBox *checkBox;
    QTableView *resultsTable;
};

#endif