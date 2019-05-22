#include <QMessageBox>
#include <QTextStream>
#include <QHeaderView>
#include "dmlTree.h"

enum { DomElementRole = Qt::UserRole + 1 };

Q_DECLARE_METATYPE(QDomElement)

static inline QString volumeElement() { return QStringLiteral("volume"); }
static inline QString materialElement() { return QStringLiteral("material"); }
static inline QString loadcaseElement() { return QStringLiteral("loadcase"); }
static inline QString simulationElement() { return QStringLiteral("simulation"); }\
static inline QString optimizationElement() { return QStringLiteral("optimization"); }

static inline QString versionAttribute() { return QStringLiteral("version"); }
static inline QString unitsAttribute() { return QStringLiteral("units"); }
static inline QString idAttribute() { return QStringLiteral("id"); }

// Volume attributes
static inline QString primitiveAttribute() { return QStringLiteral("primitive"); }
static inline QString urlAttribute() { return QStringLiteral("url"); }
static inline QString colorAttribute() { return QStringLiteral("color"); }
static inline QString alphaAttribute() { return QStringLiteral("alpha"); }
static inline QString renderingAttribute() { return QStringLiteral("rendering"); }

// Material attributes
static inline QString nameAttribute() { return QStringLiteral("name"); }
static inline QString elasticityAttribute() { return QStringLiteral("elasticity"); }
static inline QString yieldAttribute() { return QStringLiteral("yield"); }
static inline QString densityAttribute() { return QStringLiteral("density"); }

// Loadcase elements
static inline QString anchorElement() { return QStringLiteral("anchor"); }
static inline QString forceElement() { return QStringLiteral("force"); }

// Anchor attributes
static inline QString volumeAttribute() { return QStringLiteral("volume"); }

// Force attributes
static inline QString magnitudeAttribute() {return QStringLiteral("magnitude"); }
static inline QString durationAttribute() { return QStringLiteral("duration"); }

// Simulation elements
static inline QString latticeElement() {return QStringLiteral("lattice"); }
static inline QString dampingElement() {return QStringLiteral("damping"); }
static inline QString loadElement() {return QStringLiteral("load"); }
static inline QString stopElement() {return QStringLiteral("stop"); }
static inline QString repeatElement() {return QStringLiteral("repeat"); }
static inline QString planeElement() {return QStringLiteral("plane"); }

// Lattice attributes
static inline QString fillAttribute() { return QStringLiteral("fill"); }
static inline QString unitAttribute() { return QStringLiteral("unit"); }
static inline QString displayAttribute() { return QStringLiteral("display"); }
static inline QString conformAttribute() { return QStringLiteral("conform"); }
static inline QString offsetAttribute() { return QStringLiteral("offset"); }
static inline QString bardiamAttribute() { return QStringLiteral("bardiam"); }
static inline QString materialAttribute() { return QStringLiteral("material"); }
static inline QString jiggleAttribute() { return QStringLiteral("jiggle"); }

// Damping attributes
static inline QString velocityAttribute() { return QStringLiteral("velocity"); }

// Stop attributes
static inline QString criterionAttribute() { return QStringLiteral("criterion"); }
static inline QString thresholdAttribute() { return QStringLiteral("threshold"); }
static inline QString metricAttribute() { return QStringLiteral("metric"); }

// Optimization attributes + elements
static inline QString volAttribute() { return QStringLiteral("vol"); }
static inline QString ruleElement() { return QStringLiteral("rule"); }

// Rule attributes
static inline QString methodAttribute() { return QStringLiteral("method"); }
static inline QString frequencyAttribute() { return QStringLiteral("frequency"); }

// Repeat attributes
static inline QString afterAttribute() { return QStringLiteral("after"); }
static inline QString rotationAttribute() { return QStringLiteral("rotation"); }

// Plane attributes
static inline QString normalAttribute() { return QStringLiteral("normal"); }

DMLTree::DMLTree(Design *design, QWidget *parent) : QTreeWidget(parent)
{
    design_ptr = design;
    n_volumes = 0;

    this->setColumnCount(2);

    QHeaderView *header = new QHeaderView(Qt::Orientation::Horizontal, this);
    this->setHeader(header);
    this->setHeaderHidden(true);
    this->setAlternatingRowColors(true);
    this->setExpandsOnDoubleClick(false);

    header->setSectionResizeMode(QHeaderView::ResizeToContents);
    header->setSectionsClickable(true);
    header->setStretchLastSection(true);
}

bool DMLTree::read(QIODevice *device, QString fp)
{
    filePath = fp;
    QString errorStr;
    int errorLine;
    int errorColumn;

    // Attempt to parse DOM structure
    if (!domDocument.setContent(device, true, &errorStr, &errorLine,
                                &errorColumn)) {
        QMessageBox::information(window(), tr("DML Reader"),
                                 tr("Parse error at line %1, column %2:\n%3")
                                 .arg(errorLine)
                                 .arg(errorColumn)
                                 .arg(errorStr));
        return false;
    }

    // Verify DML tag and version
    QDomElement root = domDocument.documentElement();
    if (root.tagName() != "dml") {
        QMessageBox::information(window(), tr("DML Reader"), tr("The file is not an DML file."));
        return false;
    } else if (root.hasAttribute(versionAttribute())
               && root.attribute(versionAttribute()) != QLatin1String("1.0")) {
        QMessageBox::information(window(), tr("DML Reader"),
                                 tr("The file is not an DML version 1.0 file."));
        return false;
    }

    clear();

    QTreeWidgetItem *rootItem = createItem(root, nullptr);
    rootItem->setText(0, "dml");

    if (root.hasAttribute(unitsAttribute())) {
        QTreeWidgetItem *unitsItem = createItem(root.attributeNode(unitsAttribute()).toElement(), rootItem);
        unitsItem->setText(0, unitsAttribute());
        unitsItem->setText(1, root.attribute(unitsAttribute()));
    }

    //disconnect(this, &QTreeWidget::itemChanged, this, &DMLTree::updateDomElement);

    parseExpandElement(root.firstChildElement(), rootItem);
    log("Volumes: " + QString::number(n_volumes));
   // connect(this, &QTreeWidget::itemChanged, this, &DMLTree::updateDomElement);

    return true;
}

void DMLTree::parseExpandElement(const QDomElement &element,
                                  QTreeWidgetItem *parentItem)
{
    QTreeWidgetItem *item = createItem(element, parentItem);

    QString title = element.tagName();
    title = title.front().toUpper() + title.mid(1);
    qDebug() << title;
    item->setIcon(0, expandIcon);
    QString id = element.attribute(idAttribute());
    if (!id.isEmpty()) {
        item->setText(0, title + "  (" + id + ")");
    } else {
        item->setText(0, title);
    }

    setItemExpanded(item, false);

    QDomNamedNodeMap attrMap = element.attributes();

    // ---- <volume> ----
    if (element.tagName() == volumeElement()) {
        auto *id =          createAttributeItem(item, attrMap, idAttribute());
        auto *primitive =   createAttributeItem(item, attrMap, primitiveAttribute());
        auto *url =         createAttributeItem(item, attrMap, urlAttribute());
        auto *color =       createAttributeItem(item, attrMap, colorAttribute());
        auto *alpha =       createAttributeItem(item, attrMap, alphaAttribute());
        auto *rendering =   createAttributeItem(item, attrMap, renderingAttribute());
        auto *units =       createAttributeItem(item, attrMap, unitsAttribute());
        n_volumes++;

        Volume *v = new Volume(id        ? id->text(1)           : nullptr,
                              primitive ? primitive->text(1)    : nullptr,
                              url       ? filePath + "/" + url->text(1)          : nullptr,
                              units     ? units->text(1)        : nullptr,
                              rendering ? rendering->text(1)    : nullptr,
                              alpha     ? alpha->text(1)        : nullptr,
                              color     ? color->text(1)        : nullptr);
        v->index = design_ptr->volumes.size();
        design_ptr->volumes.push_back(*v);
        design_ptr->volumeMap[v->id] = v;
        log(QString("Loaded Volume: '%1'").arg(v->id));
    }

    // ---- <material> ----
    if (element.tagName() == materialElement()) {
        auto *id = createAttributeItem(item, attrMap, idAttribute());
        auto *name = createAttributeItem(item, attrMap, nameAttribute());
        auto *elast = createAttributeItem(item, attrMap, elasticityAttribute());
        auto *yield = createAttributeItem(item, attrMap, yieldAttribute());
        auto *density = createAttributeItem(item, attrMap, densityAttribute());

        Material *m = new Material();
        m->id = id ? id->text(1) : nullptr;
        m->name = name ? name->text(1) : nullptr;
        m->elasticity = elast ? elast->text(1).split(" ")[0].toDouble() : 0;
        if (elast) { m->eUnits = elast->text(1).split(" ").size() > 1 ? elast->text(1).split(" ")[1] : nullptr; }
        m->yield = yield ? yield->text(1).split(" ")[0].toDouble() : 0;
        if (yield) { m->yUnits = yield->text(1).split(" ").size() > 1 ? yield->text(1).split(" ")[1] : nullptr; }
        m->density = density ? density->text(1).split(" ")[0].toDouble() : 0;
        if (density) { m->dUnits = density->text(1).split(" ")[1].size() > 1 ? density->text(1).split(" ")[1] : nullptr; }

        m->index = design_ptr->materials.size();
        design_ptr->materials.push_back(*m);
        design_ptr->materialMap[m->id] = m;
        log(QString("Loaded Material: '%1'").arg(m->id));
    }

    // ---- <loadcase> ----
    if (element.tagName() == loadcaseElement()) {
        auto *id = createAttributeItem(item, attrMap, idAttribute());

        Loadcase *l = new Loadcase();
        l->id = id ? id->text(1) : nullptr;
        l->index = design_ptr->loadcases.size();
        design_ptr->loadcases.push_back(*l);
        design_ptr->loadcaseMap[l->id] = l;
        log(QString("Loaded Loadcase: '%1'").arg(l->id));
    }

    // ---- <anchor> ----
    if (element.tagName() == anchorElement()) {
        auto *volume = createAttributeItem(item, attrMap, volumeAttribute());

        // TODO add error checking for non-existent volume
        Anchor *a = new Anchor();
        a->volume = volume ? design_ptr->volumeMap[volume->text(1)] : nullptr;

        QString loadId = parentItem->child(0)->text(1);
        design_ptr->loadcaseMap[loadId]->anchors.push_back(*a);
        design_ptr->loadcaseMap[loadId]->anchorMap[a->volume->id] = a;
        log(QString("Loaded Anchor: '%1'").arg(a->volume->id));
    }

    // ---- <force> ----
    if (element.tagName() == forceElement()) {
        auto *volume = createAttributeItem(item, attrMap, volumeAttribute());
        auto *magnitude = createAttributeItem(item, attrMap, magnitudeAttribute());
        auto *duration = createAttributeItem(item, attrMap, durationAttribute());

        // TODO add error checking for non-existent volume
        Force *f = new Force();
        f->volume = volume ? design_ptr->volumeMap[volume->text(1)] : nullptr;
        f->magnitude = magnitude ? parseVec(magnitude->text(1)) : Vec(0, 0, 0);
        f->duration = duration ? duration->text(1).toDouble() : -1;

        QString loadId = parentItem->child(0)->text(1);
        design_ptr->loadcaseMap[loadId]->forces.push_back(*f);
        design_ptr->loadcaseMap[loadId]->forceMap[f->volume->id] = f;
        log(QString("Loaded Force: '%1'").arg(f->volume->id));
    }

    // ---- <simulation> ----
    if (element.tagName() == simulationElement()) {
        auto *id = createAttributeItem(item, attrMap, idAttribute());
        auto *volume = createAttributeItem(item, attrMap, volumeAttribute());


        SimulationConfig *s = new SimulationConfig();
        s->id = id ? id->text(1) : nullptr;
        s->volume = volume ? design_ptr->volumeMap[volume->text(1)] : nullptr;
        s->index = design_ptr->simConfigs.size();
        design_ptr->simConfigs.push_back(*s);
        design_ptr->simConfigMap[s->id] = s;
        log(QString("Loaded Simulation Config: '%1'").arg(s->id));
    }

    // ---- <lattice> ----
    if (element.tagName() == latticeElement()) {
        qDebug() << "in lattice loader";
        auto *fill = createAttributeItem(item, attrMap, fillAttribute());
        auto *unit = createAttributeItem(item, attrMap, unitAttribute());
        auto *display = createAttributeItem(item, attrMap, displayAttribute());
        auto *conform = createAttributeItem(item, attrMap, conformAttribute());
        auto *offset = createAttributeItem(item, attrMap, offsetAttribute());
        auto *bardiam = createAttributeItem(item, attrMap, bardiamAttribute());
        auto *material = createAttributeItem(item, attrMap, materialAttribute());
        auto *jiggle = createAttributeItem(item, attrMap, jiggleAttribute());

        qDebug() << "Loading lattice config";

        LatticeConfig l = LatticeConfig();
        l.fill = fill ? (fill->text(1) == "cubic_full"?
                             LatticeConfig::CUBIC_FILL :
                             LatticeConfig::SPACE_FILL) : LatticeConfig::CUBIC_FILL;
        l.unit = unit ? parseVec(unit->text(1)) : Vec(0, 0, 0);
        l.display = display ? display->text(1) : nullptr;
        l.conform = conform ? conform->text(1).toInt() : false;
        l.offset = offset ? parseVec(offset->text(1)) : Vec(0, 0, 0);
        l.barDiameter = bardiam ? parseVec(bardiam->text(1)) : Vec(0, 0, 0);
        l.material = material ? design_ptr->materialMap[material->text(1)] : nullptr;
        l.jiggle = jiggle ? parseVec(jiggle->text(1)) : Vec(0, 0, 0);

        QString simConfigId = parentItem->child(0)->text(1);
        design_ptr->simConfigMap[simConfigId]->lattice = l;
    }

    // ---- <damping> ----
    if (element.tagName() == dampingElement()) {
        auto *velocity = createAttributeItem(item, attrMap, velocityAttribute());

        Damping d = Damping();
        d.velocity = velocity ? velocity->text(1).toDouble() : 0;

        QString simConfigId = parentItem->child(0)->text(1);
        design_ptr->simConfigMap[simConfigId]->damping = d;
    }

    // ---- <repeat> ----
    if (element.tagName() == repeatElement()) {
        auto *after = createAttributeItem(item, attrMap, afterAttribute());
        auto *rotation = createAttributeItem(item, attrMap, rotationAttribute());

        Repeat r = Repeat();
        r.after = after ? after->text(1).split(" ")[0].toDouble() : -1;

        if (rotation) {
            if (rotation->text(1) == "random") {
                r.rotationExplicit = false;
            } else {
                r.rotationExplicit = true;
                r.rotation = parseVec(rotation->text(1));
            }
        } else {
            r.rotationExplicit = true;
            r.rotation = Vec(0, 0, 0);
        }

        QString simConfigId = parentItem->child(0)->text(1);
        design_ptr->simConfigMap[simConfigId]->repeat = r;
    }

    // ---- <plane> ----
    if (element.tagName() == planeElement()) {
        auto *normal = createAttributeItem(item, attrMap, normalAttribute());
        auto *offset = createAttributeItem(item, attrMap, offsetAttribute());

        Plane *p = new Plane();
        p->normal = normal ? parseVec(normal->text(1)) : Vec(0, 1, 0);
        p->offset = offset ? offset->text(1).toDouble() : 0;

        QString simConfigId = parentItem->child(0)->text(1);
        design_ptr->simConfigMap[simConfigId]->plane = p;
    }

    // ---- <load> ----
    if (element.tagName() == loadElement()) {
        auto *id = createAttributeItem(item, attrMap, idAttribute());

        Loadcase *l = id ? design_ptr->loadcaseMap[id->text(1)] : nullptr;

        QString simConfigId = parentItem->child(0)->text(1);
        design_ptr->simConfigMap[simConfigId]->load = l;
    }

    // ---- <stop> ----
    if (element.tagName() == stopElement()) {
        auto *criterion = createAttributeItem(item, attrMap, criterionAttribute());
        auto *threshold = createAttributeItem(item, attrMap, thresholdAttribute());
        auto *metric = createAttributeItem(item, attrMap, metricAttribute());

        if (parentItem->text(0).toLower().startsWith(simulationElement())) {
            Stop s = Stop();
            s.criterion = criterion ? (criterion->text(1) == "time"?
                                       Stop::SC_TIME : Stop::SC_MOTION) :
                          Stop::SC_TIME;
            s.threshold = threshold ? threshold->text(1).toDouble() : 0;

            QString simConfigId = parentItem->child(0)->text(1);
            design_ptr->simConfigMap[simConfigId]->stops.push_back(s);
        }
        if (parentItem->text(0).toLower().startsWith(optimizationElement())) {
            OptimizationStop s = OptimizationStop();
            s.metric = metric ? (metric->text(1) == "weight"?
                    OptimizationStop::WEIGHT : OptimizationStop::ENERGY) :
                            OptimizationStop::WEIGHT;
            s.threshold = threshold ? threshold->text(1).toDouble() : 100;

            if (design_ptr->optConfig != nullptr) {
                design_ptr->optConfig->stopCriteria.push_back(s);
            }
        }
    }

    // ---- <optimization> ----
    if (element.tagName() == optimizationElement()) {
        auto *sim = createAttributeItem(item, attrMap, simulationElement());

        OptimizationConfig *o = new OptimizationConfig();
        o->simulationConfig = sim ? design_ptr->simConfigMap[sim->text(1)] : nullptr;
        design_ptr->optConfig = o;

        log(QString("Loaded Optimization Config: '%1'").arg(o->simulationConfig->id));
    }

    // ---- <rule> ----
    if (element.tagName() == ruleElement()) {
        auto *method = createAttributeItem(item, attrMap, methodAttribute());
        auto *threshold = createAttributeItem(item, attrMap, thresholdAttribute());
        auto *frequency = createAttributeItem(item, attrMap, frequencyAttribute());

        OptimizationRule r = OptimizationRule();
        r.method = method ? (method->text(1) == "remove_low_stress"?
                 OptimizationRule::REMOVE_LOW_STRESS : OptimizationRule::NONE) :
                   OptimizationRule::NONE;
        if (threshold) {
            QString t = threshold->text(1);
            qDebug() << "Threshold" << t;
            if (t.endsWith(QChar('%'))) {
                r.threshold = t.remove(QChar('%')).trimmed().toDouble() / 100;
            } else {
                r.threshold = t.toDouble();
            }
        } else { r.threshold = 0; }
        r.frequency = frequency ? frequency->text(1).toInt() : 0;

        if (design_ptr->optConfig != nullptr) {
            design_ptr->optConfig->rules.push_back(r);
        }
        qDebug() << "Rules" << design_ptr->optConfig->rules.front().threshold;

    }

    QDomElement sibling = element.nextSiblingElement();
    if (!sibling.isNull()) {
        parseExpandElement(sibling, parentItem);
    }
    QDomElement child = element.firstChildElement();
    if (!child.isNull()) {
        parseExpandElement(child, item);
    }
}

QTreeWidgetItem *DMLTree::createAttributeItem(QTreeWidgetItem *parentItem, QDomNamedNodeMap attrMap, QString attrName) {
    if (!attrMap.contains(attrName)) {
        return nullptr;
    }

    QDomNode attrNode = attrMap.namedItem(attrName);

    QTreeWidgetItem *attrItem = createItem(attrNode.toElement(), parentItem);
    QString attrValue = attrNode.nodeValue();
    attrItem->setText(0, attrName);
    attrItem->setText(1, attrValue);

    return attrItem;
}

QTreeWidgetItem *DMLTree::createItem(const QDomElement &element,
                                      QTreeWidgetItem *parentItem)
{

    QTreeWidgetItem *item;
    if (parentItem) {
        item = new QTreeWidgetItem(parentItem);
    } else {
        item = new QTreeWidgetItem(this);
    }
    item->setData(0, DomElementRole, QVariant::fromValue(element));
    this->insertTopLevelItem(0, item);
    return item;
}

bool DMLTree::write(QIODevice *device) const
{
    const int IndentSize = 4;

    QTextStream out(device);
    domDocument.save(out, IndentSize);
    return true;
}

Vec DMLTree::parseVec(QString vecString) {
    double x, y, z;
    std::string vecStdString;
    const char * vecConstChar;

    vecStdString = vecString.toStdString();
    vecConstChar = vecStdString.c_str();

    if (3 == sscanf(vecConstChar, "%lf,%lf,%lf", &x, &y, &z)) {
        return Vec(x, y, z);
    }
    if (3 == sscanf(vecConstChar, "%lf, %lf, %lf", &x, &y, &z)) {
        return Vec(x, y, z);
    }
    if (3 == sscanf(vecConstChar, "%lf %lf %lf", &x, &y, &z)) {
        return Vec(x, y, z);
    }

    log(QString("Malformed DML: Expected text in the form \"value, value, value\" but got \"%1\"").arg(vecString));
    return Vec(0, 0, 0);
}