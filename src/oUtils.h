//
// Created by sw3390 on 3/2/20.
//


#ifndef DMLIDE_OUTILS_H
#define DMLIDE_OUTILS_H

#endif //DMLIDE_OUTILS_H

#include <Titan/sim.h>
#include <QDebug>

#include "utils.h"
#include "model.h"
#include <map>

class oUtils {
public:
    void static generateMassesPoisson(double minCut, map<Mass *, vector<Spring *>> mToS, vector<Vec> &lattice);
    void static generateMassesBounded(double minCut, map<Mass *, vector<Spring *>> mToS, vector<Vec> &lattice, int n, model_data *bounding);
    bool static midpointInside(Vec m1, Vec m2, model_data *bounding);
};
