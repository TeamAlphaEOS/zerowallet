#include "mainwindow.h"
#include "settings.h"


ConsolodationAddressModel::ConsolodationAddressModel(QObject *parent)
     : QAbstractTableModel(parent) {
    headers << QObject::tr("Sapling Address");
}

ConsolodationAddressModel::~ConsolodationAddressModel() {
    delete addresses;
}

void ConsolodationAddressModel::addConsolidationData(const QList<QString>& data) {
    delete addresses;
    addresses = new QList<QString>();

    //Make sure there are no dupes
    for (int i = 0; i < data.size(); i++) {
        if(!addresses->contains(data[i])) {
            addresses->append(data[i]);
        }
    }


    updateData();
}

void ConsolodationAddressModel::addAddress(const QString& addr) {
    auto newAddresses = new QList<QString>();
    std::copy(addresses->begin(), addresses->end(), std::back_inserter(*newAddresses));
    newAddresses->append(addr);

    addConsolidationData(*newAddresses);
}

void ConsolodationAddressModel::deleteAddress(const QString& addr) {

    QList<QString> existingAddress = *addresses;
    QList<QString> newAddresses;
    QList<QString>::iterator i;
    for (int i = 0; i < existingAddress.size(); i++) {
        if (addr != existingAddress[i]) {
            newAddresses.append(existingAddress[i]);
         }
    }
    addConsolidationData(newAddresses);
}


void ConsolodationAddressModel::updateData() {
    auto newAddresses = new QList<QString>();

    if (addresses  != nullptr) std::copy( addresses->begin(),  addresses->end(), std::back_inserter(*newAddresses));

    // Sort by reverse time
    std::sort(newAddresses->begin(), newAddresses->end(), [=] (auto a, auto b) {
        return a < b; // reverse sort
    });

    // And then swap out the modeldata with the new one.
    delete addresses;
    addresses = newAddresses;

    dataChanged(index(0, 0), index(addresses->size()-1, columnCount(index(0,0))-1));
    layoutChanged();
}


int ConsolodationAddressModel::rowCount(const QModelIndex&) const
{
   if (addresses == nullptr) return 0;
   return addresses->size();
}

int ConsolodationAddressModel::columnCount(const QModelIndex&) const
{
   return headers.size();
}


QVariant ConsolodationAddressModel::data(const QModelIndex &index, int role) const
{
    if (role == Qt::TextAlignmentRole)
       return QVariant(Qt::AlignLeft | Qt::AlignVCenter);

    if (role == Qt::FontRole) {
        QFont f;
        f.setBold(false);
        return f;
    }

    auto dat = addresses->at(index.row());
    if (role == Qt::DisplayRole) {
            return dat;
    }

   return QVariant();
}

QVariant ConsolodationAddressModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (role == Qt::TextAlignmentRole)
       return QVariant(Qt::AlignLeft | Qt::AlignVCenter);

    if (role == Qt::FontRole) {
        QFont f;
        f.setBold(true);
        return f;
    }

    if (role == Qt::DisplayRole && orientation == Qt::Horizontal) {
        return headers.at(section);
    }

    return QVariant();
}

QString ConsolodationAddressModel::getAddress(int row) const {
    return addresses->at(row).trimmed();
}

Settings* Settings::instance = nullptr;

Settings* Settings::init() {
    if (instance == nullptr)
        instance = new Settings();

    return instance;
}

Settings* Settings::getInstance() {
    return instance;
}

Config Settings::getSettings() {
    // Load from the QT Settings.
    QSettings s;

    auto host                         = s.value("connection/host").toString();
    auto port                         = s.value("connection/port").toString();
    auto username                     = s.value("connection/rpcuser").toString();
    auto password                     = s.value("connection/rpcpassword").toString();
    auto deletetx                     = s.value("connection/deletetx").toString();
    auto consolidation                = s.value("connection/consolidation").toString();
    auto consolidationtxfee           = s.value("connection/consolidationtxfee").toString();
    auto consolidationAddressesTemp   = s.value("connection/consolidationAddresses").toString();

    QStringList list = consolidationAddressesTemp.split(",");
    QList<QString> consolidationAddresses;
    for (int i = 0; i < list.size(); ++i) {
        consolidationAddresses.push_back(list[i]);
    }

    return Config{host, port, username, password, deletetx, consolidation, consolidationtxfee, consolidationAddresses};
}

QString Settings::locateZeroNodeConfFile() {
#ifdef Q_OS_LINUX
    auto zeroNodeConfLocation = QStandardPaths::locate(QStandardPaths::HomeLocation, ".zero/zeronode.conf");
#elif defined(Q_OS_DARWIN)
    auto zeroNodeConfLocation = QStandardPaths::locate(QStandardPaths::HomeLocation, "Library/Application Support/Zero/zeronode.conf");
#else
    auto zeroNodeConfLocation = QStandardPaths::locate(QStandardPaths::AppDataLocation, "../../zero/zeronode.conf");
#endif

    return QDir::cleanPath(zeroNodeConfLocation);
}

QString Settings::zeroNodeConfWritableLocation() {
#ifdef Q_OS_LINUX
    auto zeroNodeConfLocation = QDir(QStandardPaths::writableLocation(QStandardPaths::HomeLocation)).filePath(".zero/zeronode.conf");
#elif defined(Q_OS_DARWIN)
    auto zeroNodeConfLocation = QDir(QStandardPaths::writableLocation(QStandardPaths::HomeLocation)).filePath("Library/Application Support/Zero/zeronode.conf");
#else
    auto zeroNodeConfLocation = QDir(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)).filePath("../../zero/zeronode.conf");
#endif

    return QDir::cleanPath(zeroNodeConfLocation);
}
/**
 * Try to automatically detect a zeronode.conf file in the correct location and load parameters
 */
void Settings::autoDetectZeroNodeConf(LocalZNTableModel* localZeroNodesTableModel) {
    auto zeroNodeConfLocation = getInstance()->getZeroNodeConfLocation();

    if (zeroNodeConfLocation.isEmpty()) {
        zeroNodeConfLocation = locateZeroNodeConfFile();
    }

    if (zeroNodeConfLocation.isNull()) {
        // No Zcash file, just return with nothing
        return;
    }

    QFile file(zeroNodeConfLocation);
    if (!file.open(QIODevice::ReadOnly)) {
        qDebug() << file.errorString();
        return;
    }

    QTextStream in(&file);

    getInstance()->setUsingZeroNodeConf(zeroNodeConfLocation);

    while (!in.atEnd()) {
        QString line = in.readLine();

        QRegExp rx("[ ]");// match a comma or a space
        QStringList list = line.split(rx, QString::SkipEmptyParts);

        if (list.count() == 5) {
            bool ok;
            LocalZeroNodes newLocalZeroNode;
            newLocalZeroNode.status = "ENABLED";
            newLocalZeroNode.alias = list[0];
            if (list[0].startsWith("#"))
                newLocalZeroNode.status = "DISABLED";

            newLocalZeroNode.ipAddress = list[1];
            newLocalZeroNode.privateKey = list[2];
            newLocalZeroNode.txid = list[3];
            newLocalZeroNode.index = list[4].toLongLong(&ok);

            if (ok)
                zeroNodeSettings.push_back(newLocalZeroNode);
        }
    }

    file.close();

    localZeroNodesTableModel->addLocalZNData();
}

void Settings::saveSettings(const QString& host, const QString& port, const QString& username, const QString& password,
                            const QString& deletetx, const QString& consolidation, const QString& consolidationtxfee, const QList<QString>& consolidationAddresses) {
    QSettings s;

    s.setValue("connection/host", host);
    s.setValue("connection/port", port);
    s.setValue("connection/rpcuser", username);
    s.setValue("connection/rpcpassword", password);
    s.setValue("connection/deletetx", deletetx);
    s.setValue("connection/consolidation", consolidation);
    s.setValue("connection/consolidationtxfee", consolidationtxfee);

    QStringList list;
    for (int i=0; i < consolidationAddresses.size(); i++) {
        list.append(consolidationAddresses[i]);
    }

    QString consolidationAddressesList = list.join(",");
    s.setValue("connection/consolidationAddresses", list);

    s.sync();

    // re-init to load correct settings
    init();
}

void Settings::setUsingZcashConf(QString confLocation) {
    if (!confLocation.isEmpty())
        _confLocation = confLocation;
}

void Settings::setUsingZeroNodeConf(QString zeroNodeConfLocation) {
    if (!zeroNodeConfLocation.isEmpty())
        _zeroNodeConfLocation = zeroNodeConfLocation;
}

bool Settings::isTestnet() {
    return _isTestnet;
}

void Settings::setTestnet(bool isTestnet) {
    this->_isTestnet = isTestnet;
}

bool Settings::isSaplingAddress(QString addr) {
    if (!isValidAddress(addr))
        return false;

    return ( isTestnet() && addr.startsWith("ztestsapling")) ||
           (!isTestnet() && addr.startsWith("zs"));
}

bool Settings::isSproutAddress(QString addr) {
    if (!isValidAddress(addr))
        return false;

    return isZAddress(addr) && !isSaplingAddress(addr);
}

bool Settings::isZAddress(QString addr) {
    if (!isValidAddress(addr))
        return false;

    return addr.startsWith("z");
}

bool Settings::isTAddress(QString addr) {
    if (!isValidAddress(addr))
        return false;

    return addr.startsWith("t");
}

int Settings::getZcashdVersion() {
    return _zcashdVersion;
}

void Settings::setZcashdVersion(int version) {
    _zcashdVersion = version;
}

bool Settings::isSyncing() {
    return _isSyncing;
}

void Settings::setSyncing(bool syncing) {
    this->_isSyncing = syncing;
}

int Settings::getBlockNumber() {
    return this->_blockNumber;
}

void Settings::setBlockNumber(int number) {
    this->_blockNumber = number;
}

bool Settings::isSaplingActive() {
    return  (isTestnet() && getBlockNumber() > 0) ||
			(!isTestnet() && getBlockNumber() > 492850);
}

double Settings::getZECPrice() {
    return zecPrice;
}

bool Settings::getAutoShield() {
    // Load from Qt settings
    return QSettings().value("options/autoshield", false).toBool();
}

void Settings::setAutoShield(bool allow) {
    QSettings().setValue("options/autoshield", allow);
}

bool Settings::getCheckForUpdates() {
    return QSettings().value("options/allowcheckupdates", true).toBool();
}

void Settings::setCheckForUpdates(bool allow) {
     QSettings().setValue("options/allowcheckupdates", allow);
}

bool Settings::getAllowFetchPrices() {
    return QSettings().value("options/allowfetchprices", true).toBool();
}

void Settings::setAllowFetchPrices(bool allow) {
     QSettings().setValue("options/allowfetchprices", allow);
}

bool Settings::getAllowCustomFees() {
    // Load from the QT Settings.
    return QSettings().value("options/customfees", false).toBool();
}

void Settings::setAllowCustomFees(bool allow) {
    QSettings().setValue("options/customfees", allow);
}

QString Settings::get_theme_name() {
    // Load from the QT Settings.
    return QSettings().value("options/theme_name", "zero").toString();
}

void Settings::set_theme_name(QString theme_name) {
    QSettings().setValue("options/theme_name", theme_name);
}

bool Settings::getSaveZtxs() {
    // Load from the QT Settings.
    return QSettings().value("options/savesenttx", true).toBool();
}

void Settings::setSaveZtxs(bool save) {
    QSettings().setValue("options/savesenttx", save);
}

void Settings::setPeers(int peers) {
    _peerConnections = peers;
}

int Settings::getPeers() {
    return _peerConnections;
}
//=================================
// Static Stuff
//=================================
void Settings::saveRestore(QDialog* d) {
    d->restoreGeometry(QSettings().value(d->objectName() % "geometry").toByteArray());

    QObject::connect(d, &QDialog::finished, [=](auto) {
        QSettings().setValue(d->objectName() % "geometry", d->saveGeometry());
    });
}

void Settings::saveRestoreTableHeader(QTableView* table, QDialog* d, QString tablename) {
    table->horizontalHeader()->restoreState(QSettings().value(tablename).toByteArray());
    table->horizontalHeader()->setStretchLastSection(true);

    QObject::connect(d, &QDialog::finished, [=](auto) {
        QSettings().setValue(tablename, table->horizontalHeader()->saveState());
    });
}

void Settings::openAddressInExplorer(QString address) {
    QString url;
    if (Settings::getInstance()->isTestnet()) {
        url = "https://testnet.zero.org/address/" + address;
    } else {
        url = "https://insight.zerocurrency.io/insight/address/" + address;
    }
    QDesktopServices::openUrl(QUrl(url));
}

void Settings::openTxInExplorer(QString txid) {
    QString url;
    if (Settings::getInstance()->isTestnet()) {
        url = "https://testnet.zero.org/tx/" + txid;
    }
    else {
        url = "https://insight.zerocurrency.io/insight/tx/" + txid;
    }
    QDesktopServices::openUrl(QUrl(url));
}

QString Settings::getUSDFormat(double bal) {
    return "$" + QLocale(QLocale::English).toString(bal, 'f', 2);
}


QString Settings::getUSDFromZecAmount(double bal) {
    return getUSDFormat(bal * Settings::getInstance()->getZECPrice());
}


QString Settings::getDecimalString(double amt) {
    QString f = QString::number(amt, 'f', 8);

    while (f.contains(".") && (f.right(1) == "0" || f.right(1) == ".")) {
        f = f.left(f.length() - 1);
    }
    if (f == "-0")
        f = "0";

    return f;
}

QString Settings::getZECDisplayFormat(double bal) {
    // This is idiotic. Why doesn't QString have a way to do this?
    return getDecimalString(bal) % " " % Settings::getTokenName();
}

QString Settings::getZECUSDDisplayFormat(double bal) {
    auto usdFormat = getUSDFromZecAmount(bal);
    if (!usdFormat.isEmpty())
        return getZECDisplayFormat(bal) % " (" % usdFormat % ")";
    else
        return getZECDisplayFormat(bal);
}

const QString Settings::txidStatusMessage = QString(QObject::tr("Tx submitted (right click to copy) txid:"));

QString Settings::getTokenName() {
    if (Settings::getInstance()->isTestnet()) {
        return "ZET";
    } else {
        return "ZER";
    }
}

QString Settings::getDonationAddr() {
    if (Settings::getInstance()->isTestnet())
            return "ztestsaplingXXX";
    else
            return "t1fDbALrS7tZV7DDvadAT7yHi5Sztptj8yP";
}

bool Settings::updateToZeroNodeConf(QString confLocation, QList<QString> zeroNodes) {
    QFile file(confLocation);
    if (!file.open(QIODevice::ReadWrite | QIODevice::Truncate))
        return false;


    QTextStream out(&file);
    for (auto& it : zeroNodes) {
        out << it << "\n";
    }
    file.close();

    return true;
}

bool Settings::addToZcashConf(QString confLocation, QString line) {
    QFile file(confLocation);
    if (!file.open(QIODevice::ReadWrite | QIODevice::Append))
        return false;


    QTextStream out(&file);
    out << line << "\n";
    file.close();

    return true;
}

bool Settings::removeFromZcashConf(QString confLocation, QString option) {
    if (confLocation.isEmpty())
        return false;

    // To remove an option, we'll create a new file, and copy over everything but the option.
    QFile file(confLocation);
    if (!file.open(QIODevice::ReadOnly))
        return false;

    QList<QString> lines;
    QTextStream in(&file);
    while (!in.atEnd()) {
        QString line = in.readLine();
        auto s = line.indexOf("=");
        QString name = line.left(s).trimmed().toLower();
        if (name != option) {
            lines.append(line);
        }
    }
    file.close();

    QFile newfile(confLocation);
    if (!newfile.open(QIODevice::ReadWrite | QIODevice::Truncate))
        return false;

    QTextStream out(&newfile);
    for (QString line : lines) {
        out << line << endl;
    }
    newfile.close();

    return true;
}

double Settings::getMinerFee() {
    return 0.0001;
}

double Settings::getZboardAmount() {
    return 0.0001;
}

QString Settings::getZboardAddr() {
    if (Settings::getInstance()->isTestnet()) {
        return getDonationAddr();
    }
    else {
        return "t1fDbALrS7tZV7DDvadAT7yHi5Sztptj8yP";
    }
}

bool Settings::isValidSaplingPrivateKey(QString pk) {
    if (isTestnet()) {
        QRegExp zspkey("^secret-extended-key-test[0-9a-z]{278}$", Qt::CaseInsensitive);
        return zspkey.exactMatch(pk);
    } else {
        QRegExp zspkey("^secret-extended-key-main[0-9a-z]{278}$", Qt::CaseInsensitive);
        return zspkey.exactMatch(pk);
    }
}

bool Settings::isValidAddress(QString addr) {
    QRegExp zcexp("^z[a-z0-9]{94}$",  Qt::CaseInsensitive);
    QRegExp zsexp("^z[a-z0-9]{77}$",  Qt::CaseInsensitive);
    QRegExp ztsexp("^ztestsapling[a-z0-9]{76}", Qt::CaseInsensitive);
    QRegExp texp("^t[a-z0-9]{34}$", Qt::CaseInsensitive);

    return  zcexp.exactMatch(addr)  || texp.exactMatch(addr) ||
            ztsexp.exactMatch(addr) || zsexp.exactMatch(addr);
}

// Get a pretty string representation of this Payment URI
QString Settings::paymentURIPretty(PaymentURI uri) {
    return QString() + "Payment Request\n" + "Pay: " + uri.addr + "\nAmount: " + getZECDisplayFormat(uri.amt.toDouble())
        + "\nMemo:" + QUrl::fromPercentEncoding(uri.memo.toUtf8());
}

// Parse a payment URI string into its components
PaymentURI Settings::parseURI(QString uri) {
    PaymentURI ans;

    if (!uri.startsWith("zero:")) {
        ans.error = "Not a zero payment URI";
        return ans;
    }

    uri = uri.right(uri.length() - QString("zero:").length());

    QRegExp re("([a-zA-Z0-9]+)");
    int pos;
    if ( (pos = re.indexIn(uri)) == -1 ) {
        ans.error = "Couldn't find an address";
        return ans;
    }

    ans.addr = re.cap(1);
    if (!Settings::isValidAddress(ans.addr)) {
        ans.error = "Could not understand address";
        return ans;
    }
    uri = uri.right(uri.length() - ans.addr.length());

    if (!uri.isEmpty()) {
        uri = uri.right(uri.length() - 1); // Eat the "?"

        QStringList args = uri.split("&");
        for (QString arg: args) {
            QStringList kv = arg.split("=");
            if (kv.length() != 2) {
                ans.error = "No value argument was seen";
                return ans;
            }

            if (kv[0].toLower() == "amt" || kv[0].toLower() == "amount") {
                ans.amt = kv[1];
            } else if (kv[0].toLower() == "memo" || kv[0].toLower() == "message" || kv[0].toLower() == "msg") {
                ans.memo = QUrl::fromPercentEncoding(kv[1].toUtf8());
            } else {
                // Ignore unknown fields, since some developers use it to pass extra data.
            }
        }
    }

    return ans;
}

const QString Settings::labelRegExp("[a-zA-Z0-9\\-_]{0,40}");
