/*
 * Copyright (c) 2014 Mark Liversedge (liversedge@gmail.com)
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the Free
 * Software Foundation; either version 2 of the License, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc., 51
 * Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */


#include "PMCData.h"

#include "Athlete.h"
#include "RideCache.h"
#include "RideMetric.h"
#include "RideItem.h"
#include "Specification.h"
#include "Season.h"
#include "Seasons.h"
#include "Context.h"

#include <stdio.h>
#include <cmath>

#include <QSharedPointer>
#include <QProgressDialog>

PMCData::PMCData(Context *context, Specification spec, QString metricName, int stsDays, int ltsDays) 
    : context(context), specification_(spec), metricName_(metricName), stsDays_(stsDays), ltsDays_(ltsDays), isstale(true)
{
    // get defaults if not passed
    useDefaults = false;

    // we're not from a datafilter
    fromDataFilter = false;
    expr = NULL;

    if (ltsDays < 0) {
        QVariant lts = appsettings->cvalue(context->athlete->cyclist, GC_LTS_DAYS);
        if (lts.isNull() || lts.toInt() == 0) ltsDays_ = 42;
        else ltsDays_ = lts.toInt();
        useDefaults=true;
    }
    if (stsDays < 0) {
        QVariant sts = appsettings->cvalue(context->athlete->cyclist, GC_STS_DAYS);
        if (sts.isNull() || sts.toInt() == 0) stsDays_ = 7;
        else stsDays_ = sts.toInt();
        useDefaults=true;
    }


    refresh();
    connect(context, SIGNAL(rideAdded(RideItem*)), this, SLOT(invalidate()));
    connect(context, SIGNAL(rideDeleted(RideItem*)), this, SLOT(invalidate()));
    connect(context, SIGNAL(refreshUpdate(QDate)), this, SLOT(invalidate()));
    connect(context->athlete->rideCache, SIGNAL(itemChanged(RideItem*)), this, SLOT(invalidate()));
    connect(context->athlete->seasons, SIGNAL(seasonsChanged()), this, SLOT(invalidate()));
}

PMCData::PMCData(Context *context, Specification spec, Leaf *expr, DataFilterRuntime *df, int stsDays, int ltsDays) 
    : context(context), specification_(spec), metricName_(""), stsDays_(stsDays), ltsDays_(ltsDays), isstale(true)
{
    // get defaults if not passed
    useDefaults = false;

    // If expr is just a metric name, we can use the faster and specific case
    QString metricName = df->lookupMap.value(expr->toString(),"");
    RideMetricFactory &factory = RideMetricFactory::instance();
    if (factory.haveMetric(metricName)) {
        // use a metric name
        metricName_ = metricName;
        fromDataFilter = false;
        expr = NULL;
    } else {
        // use an expression
        fromDataFilter = true;
        this->expr = expr;
    }

    if (ltsDays < 0) {
        QVariant lts = appsettings->cvalue(context->athlete->cyclist, GC_LTS_DAYS);
        if (lts.isNull() || lts.toInt() == 0) ltsDays_ = 42;
        else ltsDays_ = lts.toInt();
        useDefaults=true;
    }
    if (stsDays < 0) {
        QVariant sts = appsettings->cvalue(context->athlete->cyclist, GC_STS_DAYS);
        if (sts.isNull() || sts.toInt() == 0) stsDays_ = 7;
        else stsDays_ = sts.toInt();
        useDefaults=true;
    }


    refresh();
    connect(context, SIGNAL(rideAdded(RideItem*)), this, SLOT(invalidate()));
    connect(context, SIGNAL(rideDeleted(RideItem*)), this, SLOT(invalidate()));
    connect(context, SIGNAL(refreshUpdate(QDate)), this, SLOT(invalidate()));
    connect(context->athlete->rideCache, SIGNAL(itemChanged(RideItem*)), this, SLOT(invalidate()));
}

void PMCData::invalidate()
{
    isstale=true;
}

void PMCData::refresh()
{
    if (!isstale) return;

    // we need to reread config if refreshing (it might have changed)
    if (useDefaults) {

        QVariant lts = appsettings->cvalue(context->athlete->cyclist, GC_LTS_DAYS);
        if (lts.isNull() || lts.toInt() == 0) ltsDays_ = 42;
        else ltsDays_ = lts.toInt();

        QVariant sts = appsettings->cvalue(context->athlete->cyclist, GC_STS_DAYS);
        if (sts.isNull() || sts.toInt() == 0) stsDays_ = 7;
        else stsDays_ = sts.toInt();
    }

    QElapsedTimer timer;
    timer.start();

    //
    // STEP ONE: What is the date range ?
    //

    // Date range needs to take into account seasons that
    // have a starting LTS/STS potentially before any rides
    QDate seed;
    foreach(Season x, context->athlete->seasons->seasons)
        if (x.getSeed() && (seed == QDate() || x.getStart() < seed))
            seed = x.getStart();
    
    // take into account any rides, some might be before
    // the start of the first defined season
    QDate first, last;
    if (context->athlete->rideCache->rides().count()) {

        // set date range - extend to a year after last ride
        first = context->athlete->rideCache->rides().first()->dateTime.date();
        last = context->athlete->rideCache->rides().last()->dateTime.date();
    }

    // what is earliest date we got ? (substract 1 day to include first ride)
    start_ = QDate(9999,12,31);
    if (seed != QDate() && seed < start_) start_ = seed;
    if (first != QDate() && first < start_) start_ = first.addDays(-1);

    // whats the latest date we got ? (and add a year for decay)
    end_ = QDate();
    if (last > seed) end_ = last.addDays(365);
    else if (seed != QDate()) end_ = seed.addDays(365);

    // back to null date if not set, just to get round date arithmetic
    if (start_ == QDate(9999,12,31)) start_ = QDate();

    // We got a valid range ?
    if (start_ != QDate() && end_ != QDate() && start_ < end_) {

        // resize arrays
        days_ = start_.daysTo(end_)+1;
        stress_.resize(days_);
        lts_.resize(days_);
        sts_.resize(days_);
        sb_.resize(days_+1); // for SB tomorrow!
        rr_.resize(days_);

        planned_stress_.resize(days_);
        planned_lts_.resize(days_);
        planned_sts_.resize(days_);
        planned_sb_.resize(days_+1); // for SB tomorrow!
        planned_rr_.resize(days_);

        expected_stress_.resize(days_);
        expected_lts_.resize(days_);
        expected_sts_.resize(days_);
        expected_sb_.resize(days_+1); // for SB tomorrow!
        expected_rr_.resize(days_);

    } else {

        // nothing to calculate
        start_= QDate();
        end_ = QDate();
        days_ = 0;
        stress_.resize(0);
        lts_.resize(0);
        sts_.resize(0);
        sb_.resize(0);
        rr_.resize(0);

        planned_stress_.resize(0);
        planned_lts_.resize(0);
        planned_sts_.resize(0);
        planned_sb_.resize(0);
        planned_rr_.resize(0);

        expected_lts_.resize(0);
        expected_sts_.resize(0);
        expected_sb_.resize(0);
        expected_rr_.resize(0);


        // give up
        return;
    }
    //qDebug()<<"refresh PMC dates:"<<metricName_<<"days="<<days_<<"start="<<start_<<"end="<<end_;

    //
    // STEP TWO What are the seedings and ride values
    //
    const double lte = (double)exp(-1.0/ltsDays_);
    const double ste = (double)exp(-1.0/stsDays_);

    // clear what's there
    stress_.fill(0);
    lts_.fill(0);
    sts_.fill(0);
    sb_.fill(0);
    rr_.fill(0);

    planned_stress_.fill(0);
    planned_lts_.fill(0);
    planned_sts_.fill(0);
    planned_sb_.fill(0);
    planned_rr_.fill(0);

    expected_stress_.fill(0);
    expected_lts_.fill(0);
    expected_sts_.fill(0);
    expected_sb_.fill(0);
    expected_rr_.fill(0);

    // add the seeded values from seasons
    foreach(Season x, context->athlete->seasons->seasons) {
        if (x.getSeed()) {
            int offset = start_.daysTo(x.getStart());
            lts_[offset] = x.getSeed() * -1;
            sts_[offset] = x.getSeed() * -1;

            planned_lts_[offset] = x.getSeed() * -1;
            planned_sts_[offset] = x.getSeed() * -1;
        }
    }

    DataFilter* df = new DataFilter(this, context);

    int todayOffset = -1;
    double todayActualStress = 0;
    double todayPlannedStress = 0;

    // add the stress scores
    foreach(RideItem *item, context->athlete->rideCache->rides()) {

        if (!specification_.pass(item)) continue;

        // seed with score for this one
        int offset = start_.daysTo(item->dateTime.date());
        if (offset > 0 && offset < stress_.count()) {

            // although metrics are cleansed, we check here because development
            // builds have a rideDB.json that has nan and inf values in it.
            double value = 0;;
            if (fromDataFilter) value = expr->eval(&df->rt, expr, Result(0), 0, item).number();
            else value = item->getForSymbol(metricName_);

            if (!std::isinf(value) && !std::isnan(value)) {
                if (item->planned)
                    planned_stress_[offset] += value;
                else
                    stress_[offset] += value;
                //qDebug()<<"stress_["<<offset<<"] :"<<stress_[offset];

                if (start_.addDays(offset).daysTo(QDate::currentDate()) == 0) {
                    // Collect todays stress separately to decide later whether to use planned or actual stress
                    todayOffset = offset;
                    if (item->planned) {
                        todayPlannedStress += value;
                    } else {
                        todayActualStress += value;
                    }
                } else if (start_.addDays(offset).daysTo(QDate::currentDate()) < 0) {
                    if (item->planned) {
                        expected_stress_[offset] += value;
                    }
                } else {
                    if (! item->planned) {
                        expected_stress_[offset] += value;
                    }
                }
            }
        }
    }
    if (todayOffset > -1) {
        // Special case today: Use actual stress if available, otherwise planned
        expected_stress_[todayOffset] = (todayActualStress > 0) ? todayActualStress : todayPlannedStress;
    }

    delete df;

    calculateMetrics(days_, stress_, lts_, sts_, sb_, rr_);
    calculateMetrics(days_, planned_stress_, planned_lts_, planned_sts_, planned_sb_, planned_rr_);
    calculateMetrics(days_, expected_stress_, expected_lts_, expected_sts_, expected_sb_, expected_rr_);

    //qDebug()<<"refresh PMC in="<<timer.elapsed()<<"ms";

    isstale=false;
}


void
PMCData::calculateMetrics
(int days, const QVector<double> &stress, QVector<double> &lts, QVector<double> &sts, QVector<double> &sb, QVector<double> &rr) const
{
    const bool sbToday = appsettings->cvalue(context->athlete->cyclist, GC_SB_TODAY).toInt();
    const double lte = (double)exp(-1.0/ltsDays_);
    const double ste = (double)exp(-1.0/stsDays_);

    double lastLTS=0.0f;
    double lastSTS=0.0f;
    double rollingStress=0;

    for(int day=0; day < days; day++) {

        // not seeded
        if (lts[day] >=0 || sts[day]>=0) {

            // LTS
            if (day) lastLTS = lts[day-1];
            lts[day] = (stress[day] * (1.0 - lte)) + (lastLTS * lte);

            // STS
            if (day) lastSTS = sts[day-1];
            sts[day] = (stress[day] * (1.0 - ste)) + (lastSTS * ste);

        } else if (lts[day]< 0 || sts[day]<0) {

            lts[day] *= -1;
            sts[day] *= -1;
        }

        // rolling stress for STS days
        if (day && day <= stsDays_) {
            // just starting out
            rollingStress += lts[day] - lts[day-1];
            rr[day] = rollingStress;
        } else if (day) {
            rollingStress += lts[day] - lts[day-1];
            rollingStress -= lts[day-stsDays_] - lts[day-stsDays_-1];
            rr[day] = rollingStress;
        }

        // SB (stress balance)  long term - short term
        // We allow it to be shown today or tomorrow where
        // most (sane/thinking) folks usually show SB on the following day
        sb[day+(sbToday ? 0 : 1)] =  lts[day] - sts[day];
    }
}

int
PMCData::indexOf(QDate date)
{
    refresh();

    // offset into arrays or -1 if invalid
    int returning = start_.daysTo(date);
    if (returning < 0 || returning >= days_)
        returning = -1;

    return returning;

}

double
PMCData::lts(QDate date)
{
    refresh();

    int index=indexOf(date);
    if (index == -1) return 0.0f;
    else return lts_[index];
}

double
PMCData::sts(QDate date)
{
    refresh();

    int index=indexOf(date);
    if (index == -1) return 0.0f;
    else return sts_[index];
}

double
PMCData::stress(QDate date)
{
    refresh();

    int index=indexOf(date);
    if (index == -1) return 0.0f;
    else return stress_[index];
}

double
PMCData::sb(QDate date)
{
    refresh();

    int index=indexOf(date);
    if (index == -1) return 0.0f;
    else return sb_[index];
}

double
PMCData::rr(QDate date)
{
    refresh();

    int index=indexOf(date);
    if (index == -1) return 0.0f;
    else return rr_[index];
}

double
PMCData::plannedLts(QDate date)
{
    refresh();

    int index=indexOf(date);
    if (index == -1) return 0.0f;
    else return planned_lts_[index];
}

double
PMCData::plannedSts(QDate date)
{
    refresh();

    int index=indexOf(date);
    if (index == -1) return 0.0f;
    else return planned_sts_[index];
}

double
PMCData::plannedStress(QDate date)
{
    refresh();

    int index=indexOf(date);
    if (index == -1) return 0.0f;
    else return planned_stress_[index];
}

double
PMCData::plannedSb(QDate date)
{
    refresh();

    int index=indexOf(date);
    if (index == -1) return 0.0f;
    else return planned_sb_[index];
}

double
PMCData::plannedRr(QDate date)
{
    refresh();

    int index=indexOf(date);
    if (index == -1) return 0.0f;
    else return planned_rr_[index];
}

double
PMCData::expectedLts(QDate date)
{
    refresh();

    int index=indexOf(date);
    if (index == -1) return 0.0f;
    else return expected_lts_[index];
}

double
PMCData::expectedSts(QDate date)
{
    refresh();

    int index=indexOf(date);
    if (index == -1) return 0.0f;
    else return expected_sts_[index];
}

double
PMCData::expectedSb(QDate date)
{
    refresh();

    int index=indexOf(date);
    if (index == -1) return 0.0f;
    else return expected_sb_[index];
}

double
PMCData::expectedRr(QDate date)
{
    refresh();

    int index=indexOf(date);
    if (index == -1) return 0.0f;
    else return expected_rr_[index];
}

// rag reporting according to wattage type groupthink
QColor PMCData::ltsColor(double value, QColor defaultColor)
{
    //if (value < 30) return QColor(Qt::red);
    //if (value < 80) return QColor(Qt::yellow);
    if (value > 75) return QColor(Qt::green);
    if (value > 100) return QColor(Qt::blue);
    return defaultColor;
}

QColor PMCData::stsColor(double, QColor defaultColor)
{
    return defaultColor; // nowt is wrong, rest or peak who can tell ?
}

QColor PMCData::sbColor(double value, QColor defaultColor)
{
    if (value < -40) return QColor(Qt::red); // injury risk
    if (value > 15) return QColor(Qt::yellow); // detraining risk
    if (value >= -5 && value < 15) return QColor(Qt::green); // ideal
    return defaultColor;
}

QColor PMCData::rrColor(double value, QColor defaultColor)
{
    if (value < -4 || value > 8) return QColor(Qt::red); // too fast or detraining
    //if (value < 0) return QColor(Qt::yellow); // risk of losing fitness
    return defaultColor;
}

// User descriptions for the 4 series
QString PMCData::ltsDescription()
{
    return tr("CTL/LTS : Chronic Training Load/Long Term Stress. The dose of training you accumulated over a longer period of time, computed as an exponentially weighted moving average of the selected Training Load metric typically from 4-8 weeks, 42 days by default. It is claimed to relate to your fitness.");
}

QString PMCData::stsDescription()
{
    return tr("ATL/STS : Acute Training Load/Short Term Stress. The dose of training that you accumulated over a short period of time, computed as an exponentially weighted moving average of the selected Training Load metric from 3 to 10 days in general, 7 by default. It is claimed to relate to your fatigue.");
}

QString PMCData::sbDescription()
{
    return tr("TSB/SB : Training Stress Balance/Stress Balance. It's the result of subtracting yesterday's Acute Training Load/Short Term Stress from yesterday's Chronic Training Load/Long Term Stress. It is claimed to relate to your freshness.");
}

QString PMCData::rrDescription()
{
    return tr("RR : Ramp Rate. The rate at which CTL/LTS increases over a given time period. Large values up and down indicate a risk of injury and aggressive taper respectively.");
}
