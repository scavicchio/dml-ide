#include "properties.h"

static inline QString unitsAttribute() { return QStringLiteral("units"); }
static inline QString idAttribute() { return QStringLiteral("id"); }
static inline QString primitiveAttribute() { return QStringLiteral("primitive"); }
static inline QString urlAttribute() { return QStringLiteral("url"); }
static inline QString colorAttribute() { return QStringLiteral("color"); }
static inline QString renderingAttribute() { return QStringLiteral("rendering"); }

PropertiesTable::PropertiesTable(Design *design, QWidget *parent)
    : QTableWidget (parent),
      displayObject(NONE),
      objectIndex(0)
{
    this->design = design;

}

void PropertiesTable::displayVolume(QString id) {
    disconnect(this, &PropertiesTable::cellChanged, this, &PropertiesTable::updateProp);

    this->clear();
    int rowCount = 0;
    displayObject = VOLUME;

    Volume *volume = design->volumeMap[id];

    objectIndex = volume->index;
    this->title->setText(QString("Volume (%1)").arg(volume->index));

    this->setRowCount(6);
    this->setColumnCount(2);

    this->horizontalHeader()->setStretchLastSection(true);
    this->horizontalHeader()->hide();
    this->verticalHeader()->hide();

    createPropertyItem(rowCount, 0, idAttribute());
    createValueItem(rowCount++, 1, volume->id);

    createPropertyItem(rowCount, 0, primitiveAttribute());
    createValueItem(rowCount++, 1, volume->primitive);

    createPropertyItem(rowCount, 0, urlAttribute());
    createValueItem(rowCount++, 1, volume->url.fileName());

    createPropertyItem(rowCount, 0, colorAttribute());
    createColorValueItem(rowCount++, 1, volume->color);

    createPropertyItem(rowCount, 0, unitsAttribute());
    createValueItem(rowCount++, 1, volume->units);

    createPropertyItem(rowCount, 0, renderingAttribute());
    createValueItem(rowCount++, 1, volume->rendering);

    connect(this, &PropertiesTable::cellChanged, this, &PropertiesTable::updateProp);
}

void PropertiesTable::displayMaterial(QString id) {
    disconnect(this, &PropertiesTable::cellChanged, this, &PropertiesTable::updateProp);

    this->clear();
    int rowCount = 0;
    displayObject = MATERIAL;

    Material *material = design->materialMap[id];

    objectIndex = material->index;
    this->title->setText(QString("Material (%1)").arg(material->index));

    this->setRowCount(5);
    this->setColumnCount(2);

    this->horizontalHeader()->setStretchLastSection(true);
    this->horizontalHeader()->hide();
    this->verticalHeader()->hide();

    createPropertyItem(rowCount, 0, idAttribute());
    createValueItem(rowCount++, 1, material->id);

    createPropertyItem(rowCount, 0, "name");
    createValueItem(rowCount++, 1, material->name);

    createPropertyItem(rowCount, 0, "elasticity");
    createValueItem(rowCount++, 1, QString::number(material->elasticity));

    createPropertyItem(rowCount, 0, "yield");
    createValueItem(rowCount++, 1, QString::number(material->yield));

    createPropertyItem(rowCount, 0, "density");
    createValueItem(rowCount++, 1, QString::number(material->density));

    connect(this, &PropertiesTable::cellChanged, this, &PropertiesTable::updateProp);
}

void PropertiesTable::displayLoadcase(QString id) {
    disconnect(this, &PropertiesTable::cellChanged, this, &PropertiesTable::updateProp);

    this->clear();
    int rowCount = 0;
    displayObject = LOADCASE;

    Loadcase *load = design->loadcaseMap[id];

    objectIndex = load->index;
    this->title->setText(QString("Loadcase (%1)").arg(load->index));

    this->setRowCount(1 + 2*load->anchors.size() + 4 *load->forces.size());
    this->setColumnCount(3);

    QStringList headerLabels = QStringList();
    headerLabels.append("Property");
    headerLabels.append("");
    headerLabels.append("");

    this->setHorizontalHeaderLabels(headerLabels);
    this->horizontalHeader()->setStretchLastSection(true);

    createPropertyItem(rowCount, 0, "id");
    createValueItem(rowCount++, 1, load->id);

    for (const Anchor &a : load->anchors) {
        createNodeItem(rowCount, 0, "anchor");
        createPropertyItem(++rowCount, 1, "volume");
        createValueItem(rowCount++, 2, a.volume->id);
    }
    for (const Force &f : load->forces) {
        createNodeItem(rowCount, 0, "force");
        createPropertyItem(++rowCount, 1, "volume");
        createValueItem(rowCount, 2, f.volume->id);
        createPropertyItem(++rowCount, 1, "magnitude");
        createVecValueItem(rowCount, 2, f.magnitude);
        createPropertyItem(++rowCount, 1, "duration");
        createValueItem(rowCount, 2, f.duration > 0 ? QString::number(f.duration) : "");
    }
    connect(this, &PropertiesTable::cellChanged, this, &PropertiesTable::updateProp);
}


void PropertiesTable::displaySimulation(QString id) {
    disconnect(this, &PropertiesTable::cellChanged, this, &PropertiesTable::updateProp);

    this->clear();
    int rowCount = 0;
    displayObject = SIMULATION;

    SimulationConfig *simConfig = design->simConfigMap[id];

    objectIndex = simConfig->index;
    this->title->setText(QString("Simulation (%1)").arg(simConfig->index));

    QStringList headerLabels = QStringList();
    headerLabels.append("Property");
    headerLabels.append("");
    headerLabels.append("");

    this->setRowCount(21 + 3 * int(simConfig->stops.size()));
    this->setColumnCount(3);

    this->setHorizontalHeaderLabels(headerLabels);
    this->horizontalHeader()->setStretchLastSection(true);

    createPropertyItem(rowCount, 0, "id");
    createValueItem(rowCount++, 1, simConfig->id);

    createPropertyItem(rowCount, 0, "volume");
    createValueItem(rowCount++, 1, simConfig->volume->id);

    createNodeItem(rowCount, 0, "lattice");
    createPropertyItem(++rowCount, 1, "fill");
    createValueItem(rowCount, 2, simConfig->lattice.fillName());
    createPropertyItem(++rowCount, 1, "unit");
    createVecValueItem(rowCount, 2, simConfig->lattice.unit);
    createPropertyItem(++rowCount, 1, "display");
    createValueItem(rowCount, 2, simConfig->lattice.display);
    createPropertyItem(++rowCount, 1, "conform");
    createValueItem(rowCount, 2, QString(simConfig->lattice.conform));
    createPropertyItem(++rowCount, 1, "offset");
    createVecValueItem(rowCount, 2, simConfig->lattice.offset);
    createPropertyItem(++rowCount, 1, "bar diameter");
    createVecValueItem(rowCount, 2, simConfig->lattice.barDiameter);
    createPropertyItem(++rowCount, 1, "material");
    createValueItem(rowCount, 2, simConfig->lattice.material->id);
    createPropertyItem(++rowCount, 1, "jiggle");
    createVecValueItem(rowCount, 2, simConfig->lattice.jiggle);

    createNodeItem(++rowCount, 0, "damping");
    createPropertyItem(++rowCount, 1, "velocity");
    createValueItem(rowCount, 2, QString::number(simConfig->damping.velocity));

    createNodeItem(++rowCount, 0, "load");
    createPropertyItem(++rowCount, 1, "id");
    createValueItem(rowCount, 2, simConfig->load->id);

    createNodeItem(++rowCount, 0, "repeat");
    createPropertyItem(++rowCount, 1, "after");
    createValueItem(rowCount, 2, QString::number(simConfig->repeat.after));
    createPropertyItem(++rowCount, 1, "rotation");
    createVecValueItem(rowCount, 2, simConfig->repeat.rotation);

    if (simConfig->plane != nullptr) {
    createNodeItem(++rowCount, 0, "plane");
    createPropertyItem(++rowCount, 1, "normal");
    createVecValueItem(rowCount, 2, simConfig->plane->normal);
    createPropertyItem(++rowCount, 1, "offset");
    createValueItem(rowCount, 2, QString::number(simConfig->plane->offset));
    }

    for (ulong s = 0; s < simConfig->stops.size(); s++) {
        createNodeItem(++rowCount, 0, "stop");
        createPropertyItem(++rowCount, 1, "criterion");
        createValueItem(rowCount, 2, simConfig->stops.at(s).criterionName());
        createPropertyItem(++rowCount, 1, "threshold");
        createValueItem(rowCount, 2, QString::number(simConfig->stops.at(s).threshold));
    }

    connect(this, &PropertiesTable::cellChanged, this, &PropertiesTable::updateProp);
}

void PropertiesTable::updateProp(int row, int col) {

    if (col > 0) {
        QTableWidgetItem *item = this->item(row, col);
        QTableWidgetItem *property = this->item(row, col - 1);
        QTableWidgetItem *parentProperty = nullptr;

        if (item == nullptr || property == nullptr) return;

        if (row > 0 && col > 1) {
            for (int r = row - 1; r >= 0; r--) {
                if (this->item(r, col-2) != nullptr) {
                    parentProperty = this->item(r, col - 2);
                    break;
                }
            }
        }

        switch (displayObject) {

            case VOLUME: {
                Volume *volume = &design->volumes[objectIndex];

                qDebug() << "Changing" << property->text() << "property";
                if (property->text() == colorAttribute()) {

                    QVector4D colorVec;
                    double r, g, b, a;
                    char * colorString = new char[item->text().length()];
                    for (int i = 0; i < item->text().length(); i++)
                        colorString[i] = item->text().at(i).toLatin1();
                    if (4 != sscanf(colorString, "(%lf, %lf, %lf, %lf)", &r, &g, &b, &a)) {
                        log(QString("Malformed DML: Expected text in the form \"(value, value, value, value)\" but got %1").arg(item->text()));
                    } else if (r < 0.0 || r > 1.0 || g < 0.0 || g > 1.0
                               || b < 0.0 || b > 1.0 || a < 0.0 || a > 1.0) {
                        log(QString("Colors mush have values between 0.0 and 1.0f. %1, %2, %3, %4")
                            .arg(r).arg(g).arg(b).arg(a));
                    } else {
                        colorVec = QVector4D(float(r), float(g), float(b), float(a));
                        volume->color = colorVec;
                        volume->updateColor();
                        updateGraphics();

                    }
                }
                break;
            }
            case MATERIAL: {
                break;
            }
            case LOADCASE: {
                break;
            }
            case SIMULATION: {
                SimulationConfig *simConfig = &design->simConfigs[objectIndex];
                qDebug() << "Changing" << property->text() << "property";
                if (parentProperty != nullptr) qDebug() << "Parent:" << parentProperty->text();
                if (property->text() == "volume") {
                    simConfig->volume = design->volumeMap[item->text()];
                }
                if (parentProperty != nullptr && parentProperty->text() == "load" && property->text() == "id") {
                    simConfig->load = design->loadcaseMap[item->text()];
                }
                if (property->text() == "fill") {
                    simConfig->lattice.fill = item->text() == "cubic" ? LatticeConfig::CUBIC_FILL : LatticeConfig::SPACE_FILL;
                }
                if (property->text() == "unit") {
                    simConfig->lattice.unit = parseVecInput(item->text());
                }
                if (property->text() == "bar diameter") {
                    simConfig->lattice.barDiameter = parseVecInput(item->text());
                }
                if (property->text() == "material") {
                    simConfig->lattice.material = design->materialMap[item->text()];
                }
                if(property->text() == "velocity") {
                    simConfig->damping.velocity = item->text().toDouble();
                }
                break;
            }
            default: {
                return;
            }
        }
    }
}


void PropertiesTable::createPropertyItem(int row, int col, QString name) {

    QTableWidgetItem *item = new QTableWidgetItem(name);
    item->setFlags(Qt::ItemIsEnabled);
    this->setItem(row, col, item);

}

void PropertiesTable::createValueItem(int row, int col, QString name) {

    QTableWidgetItem *item = new QTableWidgetItem(name);
    this->setItem(row, col, item);

}

void PropertiesTable::createNodeItem(int row, int col, QString name) {

    QTableWidgetItem *mainItem = new QTableWidgetItem(name);
    mainItem->setFlags(Qt::ItemIsEnabled);

    QFont nodeFont = mainItem->font();
    nodeFont.setBold(true);
    mainItem->setFont(nodeFont);
    mainItem->setBackgroundColor(QColor(220, 220, 220));

    this->setItem(row, col, mainItem);

    for (int c = col+1; c < this->columnCount(); c++) {
        createBlankItem(row, c);
    }
}

void PropertiesTable::createBlankItem(int row, int col) {

    QTableWidgetItem *item = new QTableWidgetItem();
    item->setFlags(Qt::NoItemFlags);
    item->setBackgroundColor(QColor(220, 220, 220));
    this->setItem(row, col, item);
}

void PropertiesTable::createColorValueItem(int row, int col, QVector4D colorVec) {

    QString colorName = QString("(%1, %2, %3, %4)").arg(double(colorVec.x()))
                                                   .arg(double(colorVec.y()))
                                                   .arg(double(colorVec.z()))
                                                   .arg(double(colorVec.w()));

    QTableWidgetItem *colorItem = new QTableWidgetItem(colorName);
    this->setItem(row, col, colorItem);
}

void PropertiesTable::createVecValueItem(int row, int col, Vec vec) {

    QString vecString = QString("(%1, %2, %3)").arg(vec[0]).arg(vec[1]).arg(vec[2]);
    QTableWidgetItem *vecitem = new QTableWidgetItem(vecString);
    this->setItem(row, col, vecitem);
}


void PropertiesTable::setLabel(QLabel *title) {

    this->title = title;
}


Vec PropertiesTable::parseVecInput(QString vecString) {
    double x, y, z;

    std::string vecStdString;
    const char * vecConstChar;

    vecStdString = vecString.toStdString();
    vecConstChar = vecStdString.c_str();

    if (3 == sscanf(vecConstChar, "(%lf, %lf, %lf)", &x, &y, &z)) {
        return Vec(x, y, z);
    }

    log(QString("Malformed input: Expected text in the form \"(value, value, value)\" but got \"%1\"").arg(vecString));
    return Vec(0, 0, 0);
}





PropertiesModel::PropertiesModel(QObject *parent)
    : QAbstractTableModel (parent),
      displayObject(NONE)
{

}

void PropertiesModel::setDesign(Design *design)
{
    beginResetModel();
    this->design = design;
    endResetModel();
}

//  ---- rowCount() ----------------------------------------------------------
//  Manually return row counts based on class attributes
//  --------------------------------------------------------------------------
int PropertiesModel::rowCount(const QModelIndex & /* parent */) const {

    switch(displayObject) {

    case DESIGN:
    {
        return 2;
    }
    case VOLUME:
    {
        return 6;
    }
    case LOADCASE:
    {
        return 6;
    }
    case SIMULATION:
    {
        return 4; // TODO: + stops
    }
    case NONE:
    {
        return 0;
    }

    }
}

//  ---- columnCount() ----------------------------------------------------------
//  Manually return column counts based on class attributes
//  --------------------------------------------------------------------------
int PropertiesModel::columnCount(const QModelIndex & /* parent */) const {

    return 2;
}

QVariant PropertiesModel::data(const QModelIndex &index, int role) const {

    if (!index.isValid() || role != Qt::DisplayRole)
        return QVariant();

    int row = index.row();
    int col = index.column();

    switch(displayObject) {}
}




