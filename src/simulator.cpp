#include "simulator.h"
#include <QDir>

Simulator::Simulator(Simulation *sim, Loader *loader, SimulationConfig *config, OptimizationConfig *optConfig) {
    this->sim = sim;
    this->config = config;
    this->optConfig = optConfig;
    this->loader = loader;
    this->optimizer = nullptr;

    n_masses = long(sim->masses.size());
    n_springs = long(sim->springs.size());
    n_planes = long(sim->planes.size());
    n_masses_start = long(sim->masses.size());
    n_springs_start = long(sim->springs.size());
    totalMass_start = sim->getTotalMass();
    deflectionPoint_start = getDeflectionPoint();
    totalEnergy = 0;
    totalLength = 0;
    totalLength_start = 0;
    totalEnergy_start = 0;
    springInserter = nullptr;
    springRemover = nullptr;
    massDisplacer = nullptr;

    double pi = atan(1.0)*4;
    for (Spring *s : sim->springs) {
        totalLength_start += s->_rest;
    }

    steps = 0;

    simStatus = PAUSED;
    dataDir = "data";
    createDataDir();

    n_repeats = 0;
    repeatTime = config->repeat.after;
    explicitRotation = config->repeat.rotationExplicit;
    repeatRotation = config->repeat.rotation;
    optimizeAfter = (repeatTime > 0)? 10 : 0;

    double minUnitDist = DBL_MAX;
    for (Spring *t : sim->springs) {
        minUnitDist = fmin(minUnitDist, t->_rest);
    }
    qDebug() << "Min unit distance" << minUnitDist;

    relaxation = 4000;

    if (optConfig != nullptr) {
        for (OptimizationRule r : optConfig->rules) {
            switch(r.method) {
                case OptimizationRule::REMOVE_LOW_STRESS:
                    this->optimizer = new SpringRemover(sim, r.threshold);
                    qDebug() << "Created SpringRemover" << r.threshold;
                    break;

                case OptimizationRule::MASS_DISPLACE: {
                    massDisplacer = new MassDisplacer(sim, config->lattice.unit[0] * 0.2, r.threshold);
                    massDisplacer->maxLocalization = minUnitDist + 1E-4;
                    massDisplacer->order = 0;
                    massDisplacer->chunkSize = 0;
                    massDisplacer->relaxation = relaxation;
                    this->optimizer = massDisplacer;
                    qDebug() << "Created MassDisplacer" << r.threshold;
                    break;
                }

                case OptimizationRule::NONE:
                    optimizer = nullptr;
                    break;
            }
        }
    }
    qDebug() << "Set optimizations";

    springRemover = new SpringRemover(sim, 0.05);
    //optimizer = new MassDisplacer(sim, 0.2);
    //springInserter = new SpringInserter(sim, 0.001);
    //springInserter->cutoff = 3.5 * config->lattice.unit[0];

    // LOAD QUEUE
    currentLoad = 0;
    pastLoadTime = 0;
    varyLoad = false;
    if (config->load != nullptr) {
        for (Force *f : config->load->forces) {
            if (!(f->vary == Vec(0,0,0))) varyLoad = true;
        }
    }
    for (Loadcase *l : config->loadQueue) {
        qDebug() << "Force masses" << l->forces.front()->masses.size();
        for (Force *f : l->forces) {
            if (!(f->vary == Vec(0,0,0))) varyLoad = true;
        }
    }

    center = getSimCenter();

    equilibrium = false;
    optimized = 0;
    closeToPrevious = 0;
    prevEnergy = -1;
    prevSteps = 0;
    switched = false;

    qDebug() << "Initialized Simulator";
}

Simulator::~Simulator() {
    delete springInserter;
    delete springRemover;
    delete massDisplacer;
}

// --------------------------------------------------------------------
// SIMULATION CONTROLS
// --------------------------------------------------------------------

void Simulator::setSyncTimestep(double st) {
    renderTimeStep = st;
}

void Simulator::setSimTimestep(double dt) {
    sim->setAllDeltaTValues(dt);
}

void Simulator::runSimulation(bool running) {
    if (running) {
        if (simStatus != STARTED) {
            sim->initCudaParameters();
        }
        simStatus = STARTED;
        run();
        printStatus();
    } else {
        simStatus = PAUSED;
    }
}

void Simulator::runStep() {
    simStatus = PAUSED;
    sim->step(sim->masses.front()->dt);
    sim->getAll();
}

void Simulator::getSimMetrics(sim_metrics &metrics) {
    metrics.time = sim->time();
    metrics.nbars = sim->springs.size();
    metrics.totalLength = totalLength;
    metrics.totalEnergy = totalEnergy;
    metrics.totalLength_start = totalLength_start;
    metrics.totalEnergy_start = totalEnergy_start;
    metrics.deflection = calcDeflection();
    metrics.relaxation_interval = relaxation;
    metrics.optimize_iterations = optimized;
    metrics.optimize_rule = (!optConfig->rules.empty())? optConfig->rules.front() : OptimizationRule();
    metrics.displacement = massDisplacer? massDisplacer->dx : 0;
}

void Simulator::exportSimulation() {
    bar_data *bd = new bar_data();
    loader->loadBarsFromSim(sim, bd, false, false);

    if (!config->output) {
        config->output = new output_data();
    }
    config->output->barData = bd;
    qDebug() << "Saved" << bd->bars.size() << "bars from simulation";

    auto t = std::time(nullptr);
    auto tm = *std::localtime(&t);
    ostringstream oss;
    oss << put_time(&tm, "%d-%m-%Y_%H-%M-%S");
    string tmString = oss.str();

    qDebug() << "Starting export thread";
    exportThread.startExport(tmString + ".stl",
                             config->output,
                             sim->springs[0]->_diam * 0.5,
                             sim->springs[0]->_diam,
                             32);
}

void Simulator::run() {

    if (!sim->running()) {
        qDebug() << "Next Load" << currentLoad << "Queue size" << config->loadQueue.size() << "Switch at time" << pastLoadTime;
        bool loadQueueDone = false;

        // Set repeats
        if (repeatTime > 0 && repeatTime < sim->time()) {
            repeatLoad();
        }

        writeMetric(metricFile);

        bool currLoadDone = sim->time() >= pastLoadTime;
        if (currLoadDone && !config->loadQueue.empty()) {
            // Load queue
            if (currentLoad >= config->loadQueue.size()) {
                loadQueueDone = true;
            } else {
                Loadcase *load = config->loadQueue[currentLoad];
                clearLoads();
                applyLoad(load);
                currentLoad++;
                pastLoadTime += load->totalDuration;
            }
        }

        qDebug() << "About to step" << sim;
        sim->step(renderTimeStep);
        qDebug() << "Stepped" << steps << "Repeats" << n_repeats;
        sim->getAll();
        qDebug() << "Synced to CPU";
        totalLength = 0;
        for (Spring *s: sim->springs) {
            totalLength += s->_rest;
        }

        bool stopReached = stopCriteriaMet();

        if (!optimized) {
            if (varyLoad) {
                varyLoadDirection();
            }
        }

        bool equilibriumMetric = optConfig != nullptr && optConfig->rules.front().method == OptimizationRule::MASS_DISPLACE;
        if (equilibriumMetric && totalEnergy / totalEnergy_start < 0.1) {
            equilibriumMetric = false;
            optimizer = springRemover;
            switched = true;
            //emit stopCriteriaSat();
        }
        if (equilibriumMetric) {

            equilibriate();

            if (optimizeAfter <= n_repeats && equilibrium && !stopReached) {

                if (optimizer != nullptr) {
                    if (!optimized) {
                        writeMetricHeader(metricFile);
                        writeCustomMetricHeader(customMetricFile);
                    }

                    qDebug() << "About to optimize";
                    optimizer->optimize();
                    equilibrium = false;
                    closeToPrevious = 0;

                    if (varyLoad) {
                        varyLoadDirection();
                    }

                    writeMetric(metricFile);
                    if (optimized == 0)
                        writeCustomMetric(customMetricFile);
                    optimized++;
                }

                prevSteps = 0;
            }
            prevEnergy = totalEnergy;

        } else {

            if (optConfig != nullptr) {
                if (switched) {
                    optimizer->optimize();
                    //writeMetric(metricFile);

                    optimized++;

                    n_masses = int(sim->masses.size());
                    n_springs = int(sim->springs.size());
                    prevSteps = 0;


                    currentLoad = 0;
                } else {
                    for (OptimizationRule r : optConfig->rules) {
                        if ((loadQueueDone || config->repeat.afterExplicit) && optimizeAfter <= n_repeats &&
                            prevSteps >= r.frequency && !stopReached) {

                            if (!optimized) {
                                writeMetricHeader(metricFile);
                            }

                            optimizer->optimize();
                            //writeMetric(metricFile);

                            optimized++;


                            n_masses = int(sim->masses.size());
                            n_springs = int(sim->springs.size());
                            prevSteps = 0;

                            currentLoad = 0;
                        }
                    }
                }
            }
        }


        steps += long(renderTimeStep / sim->masses.front()->dt);
        prevSteps += long(renderTimeStep / sim->masses.front()->dt);
        qDebug() << steps;

        if (stopReached) {
            simStatus = STOPPED;
            exportSimulation();
            return;
        }
    }
}


void Simulator::repeatLoad() {

    if (!sim->running()) {

        // Get rotation
        Vec rotation;
        if (explicitRotation) {
            rotation = repeatRotation;
        } else {
            // Random
            rotation = Utils::randDirectionVec();
        }

        // Set masses back to original positions
        for (Mass *m : sim->masses) {
            m->pos = m->origpos;

            // rotate positions
            QVector4D p = QVector4D(m->pos[0], m->pos[1], m->pos[2], 0);
            p -= QVector4D(center[0], center[1], center[2], 1);

            QMatrix4x4 rot;
            rot.setToIdentity();
            rot.rotate(rotation[0] * 360, 1.0f, 0.0f, 0.0f);
            rot.rotate(rotation[1] * 360, 0.0f, 1.0f, 0.0f);
            rot.rotate(rotation[2] * 360, 0.0f, 0.0f, 1.0f);
            p = rot * p;

            p += QVector4D(center[0], center[1], center[2], 1);
            m->pos = Vec(p.x(), p.y(), p.z());
            m->vel = Vec(0, 0, 0);
            m->acc = Vec(0, 0, 0);
        }

        // Increase repeat time
        repeatTime += config->repeat.after;

        sim->setAll();
        n_repeats++;
    }
}

Vec Simulator::getSimCenter() {
    Vec minCorner;
    Vec maxCorner;

    double maxX = -numeric_limits<double>::max();
    double maxY = -numeric_limits<double>::max();
    double maxZ = -numeric_limits<double>::max();
    double minX = numeric_limits<double>::max();
    double minY = numeric_limits<double>::max();
    double minZ = numeric_limits<double>::max();

    for (Mass *m : sim->masses) {
        maxX = std::max(maxX, m->pos[0]);
        maxY = std::max(maxY, m->pos[1]);
        maxZ = std::max(maxZ, m->pos[2]);
        minX = std::min(minX, m->pos[0]);
        minY = std::min(minY, m->pos[1]);
        minZ = std::min(minZ, m->pos[2]);
    }
    minCorner = Vec(minX, minY, minZ);
    maxCorner = Vec(maxX, maxY, maxZ);

    return 0.5 * (minCorner + maxCorner);
}

void Simulator::equilibriate() {
    totalEnergy = 0;
    for (Spring *s : sim->springs) {
        totalEnergy += s->_curr_force * s->_curr_force / s->_k;
    }
    qDebug() << "ENERGY" << totalEnergy << prevEnergy << closeToPrevious;
    if (prevEnergy > 0 && fabs(prevEnergy - totalEnergy) < totalEnergy * 1E-6) {
        closeToPrevious++;
    } else {
        closeToPrevious = 0;
    }
    if (closeToPrevious > 10) {
        equilibrium = true;
        if (!optimized) {
            totalEnergy_start = totalEnergy;
            writeMetricHeader(metricFile);
            writeCustomMetricHeader(customMetricFile);
        }
    }
}

bool Simulator::stopCriteriaMet() {
    bool stopReached = false;
    for (auto s : optConfig->stopCriteria) {
        switch (s.metric) {
            case OptimizationStop::ENERGY:
                stopReached = totalEnergy / totalEnergy_start <= s.threshold;
                break;
            case OptimizationStop::WEIGHT:
                stopReached = totalLength / totalLength_start <= s.threshold;
                break;
            case OptimizationStop::DEFLECTION:
                stopReached = calcDeflection() >= s.threshold;
                break;
            case OptimizationStop::NONE:
                stopReached = false;
                break;
        }
    }
    return stopReached;
}

double Simulator::calcDeflection() {
    Vec deflectionPoint = Vec(0,0,0);
    vector<Mass *> points = vector<Mass *>();
    if (config->load && !config->load->forces.empty()) {
        for (Force *f : config->load->forces) {
            for (Mass *m : f->masses) {
                points.push_back(m);
            }
        }
        for (Mass *p : points) {
            deflectionPoint += p->pos;
        }
        deflectionPoint[0] /= points.size();
        deflectionPoint[1] /= points.size();
        deflectionPoint[2] /= points.size();
    }

    return (deflectionPoint - getDeflectionPoint()).norm();
}

Vec Simulator::getDeflectionPoint() {
    Vec deflectionPoint = Vec(0,0,0);
    vector<Mass *> points = vector<Mass *>();
    if (config->load && !config->load->forces.empty()) {
        for (Force *f : config->load->forces) {
            for (Mass *m : f->masses) {
                points.push_back(m);
            }
        }
        for (Mass *p : points) {
            deflectionPoint += p->origpos;
        }
        deflectionPoint[0] /= points.size();
        deflectionPoint[1] /= points.size();
        deflectionPoint[2] /= points.size();
    }

    return deflectionPoint;
}

void Simulator::createDataDir() {
    QString currentPath = QDir::currentPath();
    qDebug() << currentPath;
    QDir data(currentPath + QDir::separator() + dataDir);
    if (!data.exists()) {
        qDebug() << "Data folder does not exist. Creating...";
        QDir::current().mkdir(dataDir);
    } else {
        data.removeRecursively();
        qDebug() << QDir::current().path();
        QDir::current().mkdir(dataDir);
    }

    // Create metric file
    metricFile = QString(QDir::currentPath() + QDir::separator() +
                        dataDir + QDir::separator() +
                        "optMetrics.csv");
    customMetricFile = QString(QDir::currentPath() + QDir::separator() +
                               dataDir + QDir::separator() +
                               "outsideForces.csv");
}

void Simulator::writeMetricHeader(const QString &outputFile) {
    QFile file(outputFile);
    if (!file.open(QFile::WriteOnly | QFile::Text)) {
        return;
    }

    if (optConfig->stopCriteria.front().metric == OptimizationStop::ENERGY) {
        file.write("Time,Iteration,Deflection,Displacement,Attempts,Total Energy,Total Weight\n");
    } else {
        file.write("Time,Iteration,Deflection,Total Weight,Bar Number\n");
    }

    // Write starting line
    writeMetric(outputFile);
}

void Simulator::writeCustomMetricHeader(const QString &outputFile) {
    QFile file(outputFile);
    if (!file.open(QFile::WriteOnly | QFile::Text)) {
        return;
    }

    if (optConfig->stopCriteria.front().metric == OptimizationStop::ENERGY) {
        file.write(massDisplacer->customMetricHeader.toUtf8());
    }
}

void Simulator::writeMetric(const QString &outputFile) {

    QFile file(outputFile);
    if (!file.open(QFile::WriteOnly | QIODevice::Append | QFile::Text)) {
        return;
    }

    if (optConfig->stopCriteria.front().metric == OptimizationStop::ENERGY) {
        QString mLine = QString("%1,%2,%3,%4,%5,%6,%7\n")
                .arg(sim->time())
                .arg(optimized)
                .arg(calcDeflection())
                .arg(massDisplacer->dx)
                .arg(massDisplacer->attempts)
                .arg(totalEnergy)
                .arg(totalLength);
        file.write(mLine.toUtf8());
    } else if (optConfig->stopCriteria.front().metric == OptimizationStop::WEIGHT) {
        QString mLine = QString("%1,%2,%3,%4,%5\n")
                .arg(sim->time())
                .arg(optimized)
                .arg(calcDeflection())
                .arg(totalLength)
                .arg(n_springs);
        file.write(mLine.toUtf8());
    }
}

void Simulator::writeCustomMetric(const QString &outputFile) {
    QFile file(outputFile);
    if (!file.open(QFile::WriteOnly | QIODevice::Append | QFile::Text)) {

        return;
    }

    if (optConfig->stopCriteria.front().metric == OptimizationStop::ENERGY) {
        file.write(massDisplacer->customMetric.toUtf8());
    }
}

void Simulator::printStatus() {
    sim_metrics metrics;
    getSimMetrics(metrics);
    ostringstream oss;
    oss << "-------------------------------------------------------\r";
    oss << "Simulating...\r";
    oss << "Optimization Iterations: " << metrics.optimize_iterations << "\r";
    oss << "Time: " << metrics.time << " s\r";
    oss << "-------------------------------------------------------\r";
    std::cout << oss.str();
}


// --------------------------------------------------------------------
// LOAD CONTROLS
// --------------------------------------------------------------------

void Simulator::clearLoads() {
    for (Mass *m : sim->masses) {
        m->extforce = Vec(0,0,0);
        m->extduration = 0;
        m->unfix();
    }
}

void Simulator::applyLoad(Loadcase *load) {
    sim->getAll();

    qDebug() << "Applying" << load->anchors[0]->masses.size() << "anchors and" << load->forces[0]->masses.size() << "forces";
    for (Mass *m : sim->masses) {
        for (Anchor *a : load->anchors) {
            for (Mass *am : a->masses) {
                if (m == am) {
                    m->fix();
                }
            }
        }
    }
    for (Force *f : load->forces) {
        int forceMasses = 0;

        for (Mass *fm : f->masses) {
            bool valid = false;
            for (Mass *m : sim->masses) {
                    if (m == fm) {
                    m->extduration += f->duration;
                    if (m->extduration < 0) {
                        m->extduration = DBL_MAX;
                    }
                    forceMasses ++;
                    valid = true;
                }
            }
            if (!valid) {
                f->masses.erase(remove(f->masses.begin(), f->masses.end(), fm), f->masses.end());
            }
        }
        if (forceMasses > 0) {
            Vec distributedForce = f->magnitude / forceMasses;
            for (Mass *fm : f->masses) {
                fm->extforce += distributedForce;
                fm->force += distributedForce;
            }
        }
    }
    sim->setAll();
}

void Simulator::varyLoadDirection() {

    Loadcase *l = nullptr;
    if (!config->loadQueue.empty()) {
        l = currentLoad > 0? config->loadQueue[currentLoad - 1] : config->loadQueue.back();
    } else if (config->load != nullptr) {
        l = config->load;
    }

    if (l != nullptr) {
        for (Mass *m : sim->masses) {
            m->extforce = Vec(0,0,0);
        }
        for (Force *f : l->forces) {

            qDebug() << f->vary[0] << f->vary[1] << f->vary[2];
            if (!(f->vary == Vec(0, 0, 0))) {
                double distributedMag = (f->magnitude / f->masses.size()).norm();
                Vec forceDir = f->magnitude.normalized();

                Vec dForceDir = Vec(Utils::randFloat(-f->vary[0],f->vary[0]),
                                Utils::randFloat(-f->vary[1],f->vary[1]),
                                Utils::randFloat(-f->vary[2],f->vary[2]));
                forceDir = (forceDir + dForceDir).normalized();
                qDebug() << "Varying Load" << forceDir[0] << forceDir[1] << forceDir[2];

                for (Mass *fm : f->masses) {
                    assert(fm != nullptr);

                    fm->extforce += distributedMag * forceDir;
                }
            }
        }
        sim->setAll();
    }
}