/*
 * Copyright (c) 2010 Mark Liversedge (liversedge@gmail.com)
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

#include "SpecialFields.h"
#include "RideMetric.h"
#include "Utils.h"

#include <QTextEdit>

SpecialFields::SpecialFields()
{
    reloadFields();
}

void
SpecialFields::reloadFields() {

    // Clear datasets for refresh case
    namesmap.clear();
    specialset.clear();
    metricmap.clear();

    namesmap.insert("Start Date", tr("Start Date"));                 // linked to RideFile::starttime
    specialset.insert("Start Date");
    namesmap.insert("Start Time", tr("Start Time"));                 // linked to RideFile::starttime
    specialset.insert("Start Time");
    namesmap.insert("Identifier", tr("Identifier"));                 // linked to RideFile::id
    specialset.insert("Identifier");
    namesmap.insert("Recording Interval", tr("Recording Interval")); // linked to RideFile::recIntSecs
    specialset.insert("Recording Interval");
    namesmap.insert("Change History", tr("Change History"));         // set by RideFileCommand
    specialset.insert("Change History");
    namesmap.insert("Calendar Text", "Calendar Text");               // set by openRideFile and rideMetadata DO NOT TRANSLATE
    specialset.insert("Calendar Text");
    namesmap.insert("Source Filename", tr("Source Filename"));       // set by openRideFile
    specialset.insert("Source Filename");
    namesmap.insert("Athlete", tr("Athlete"));                       // athlete name
    specialset.insert("Athlete");
    namesmap.insert("Data", tr("Data"));                             // data present
    specialset.insert("Data");
    namesmap.insert("File Format", tr("File Format"));               // file format
    specialset.insert("File Format");
    namesmap.insert("Filename", tr("Filename"));                     // filename
    specialset.insert("Filename");
    namesmap.insert("Month", tr("Month"));                           // month name
    specialset.insert("Month");
    namesmap.insert("Weekday", tr("Weekday"));                       // weekday name
    specialset.insert("Weekday");
    namesmap.insert("Year", tr("Year"));                             // year
    specialset.insert("Year");

    namesmap.insert("Route", tr("Route"));                           // GPS map Route tag
    namesmap.insert("Sport", tr("Sport"));                           // Sport Code
    namesmap.insert("SubSport", tr("SubSport"));                     // SubSport Code
    namesmap.insert("Workout Code", tr("Workout Code"));             // Workout Code
    namesmap.insert("Workout Title", tr("Workout Title"));           // Workout Title
    namesmap.insert("Weight", tr("Weight"));                         // Weight tag
    namesmap.insert("RPE", tr("RPE"));                               // RPE tag
    namesmap.insert("Commute", tr("Commute"));                       // Commute tag
    namesmap.insert("Trainer", tr("Trainer"));                       // Trainer tag
    namesmap.insert("Feel", tr("Feel"));                             // Feel tag
    namesmap.insert("Objective", tr("Objective"));                   // Objective tag
    namesmap.insert("Keywords", tr("Keywords"));                     // Keywords tag
    namesmap.insert("Equipment", tr("Equipment"));                   // Equipment tag
    namesmap.insert("Device", tr("Device"));                         // Device tag
    namesmap.insert("Device Info", tr("Device Info"));               // Device Info tag
    namesmap.insert("Pool Length", tr("Pool Length"));               // Pool Length tag

    namesmap.insert("VO2max detected", tr("VO2max detected"));       // Garmin VO2max detected
    namesmap.insert("Aerobic Training Effect", tr("Aerobic Training Effect"));// Garmin Aerobic Training Effect
    namesmap.insert("Anaerobic Training Effect", tr("Anaerobic Training Effect"));// Garmin Anaerobic Training Effect
    namesmap.insert("EPOC", tr("EPOC"));                             // Garmin EPOC
    namesmap.insert("Recovery Time", tr("Recovery Time"));           // Garmin Recovery Time
    namesmap.insert("Performance Condition", tr("Performance Condition"));// Garmin Performance Condition
    namesmap.insert("LTHR detected", tr("LTHR detected"));           // Garmin LTHR detected
    namesmap.insert("LTS detected", tr("LTS detected"));             // Garmin LTS detected
    namesmap.insert("Interval Notes", tr("Interval Notes"));         // Interval Notes for the new Interval tab
    namesmap.insert("Interval Goal", tr("Interval Goal"));           // Interval Goal for the new Interval tab
    namesmap.insert("Spike Time", tr("Spike Time"));                 // Spike Time from FixSpikes DP
    namesmap.insert("Spikes", tr("Spikes"));                         // Spikes from FixSpikes DP

    // now add all the metric fields (for metric overrides)
    const RideMetricFactory &factory = RideMetricFactory::instance();

    for (int i=0; i<factory.metricCount(); i++) {
        const RideMetric *add = factory.rideMetric(factory.metricName(i));

        QString name(Utils::unprotect(add->name()));
        QString internal(Utils::unprotect(add->internalName()));
        // add->internalName() used for compatibility win metadata.xml, could be replaced by factory.metricName(i) or add->symbol()
        namesmap.insert(internal, name);
        metricmap.insert(internal, add);
    }
}

bool
SpecialFields::isSpecial(const QString &name) const
{
    return specialset.contains(name);
}

bool
SpecialFields::isUser(const QString &name) const
{
    return !isSpecial(name) && !isMetric(name);
}

bool
SpecialFields::isMetric(const QString &name) const
{
    return (metricSymbol(name) != "");
}

QString
SpecialFields::makeTechName(const QString &name) const
{
    // strip spaces and only keep alpha values - everything else
    // becomes an underscore
    QString s = name;
    return s.replace(QRegularExpression("[^0-9A-Za-z]"), "_");
}

QString
SpecialFields::metricSymbol(const QString &name) const
{
    // return technical name for metric long name
    const RideMetric *metric = metricmap.value(name, NULL);
    if (metric) return metric->symbol();
    else return ("");
}

const RideMetric *
SpecialFields::rideMetric(const QString &name) const
{
    return metricmap.value(name, NULL);
}

QString
SpecialFields::displayName(const QString &name) const
{
    // return localized name for display
    if (namesmap.contains(name)) return namesmap.value(name);
    else return(name);
}

QString
SpecialFields::internalName(const QString &displayName) const
{
    // return internal name for storage
    QMapIterator<QString, QString> i(namesmap);
    while (i.hasNext()) {
        i.next();
        if (i.value() == displayName) return i.key();
    }
    return(displayName);
}

SpecialTabs::SpecialTabs()
{
    namesmap.insert("Workout", tr("Workout"));
    namesmap.insert("Notes", tr("Notes"));
    namesmap.insert("Metric", tr("Metric"));
    namesmap.insert("Extra", tr("Extra"));
    namesmap.insert("Raw Data", tr("Raw Data"));
    namesmap.insert("Device", tr("Device"));
    namesmap.insert("Athlete", tr("Athlete"));
    namesmap.insert("Interval", tr("Interval"));
}

QString
SpecialTabs::displayName(const QString &internalName) const
{
    // return localized name for display
    if (namesmap.contains(internalName)) return namesmap.value(internalName);
    else return(internalName);
}

QString
SpecialTabs::internalName(const QString &displayName) const
{
    // return internal name for storage
    QMapIterator<QString, QString> i(namesmap);
    while (i.hasNext()) {
        i.next();
        if (i.value() == displayName) return i.key();
    }
    return(displayName);
}

