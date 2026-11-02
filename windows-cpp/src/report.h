#pragma once
#include <QString>
#include <QVector>

#include "models.h"

struct InsurerInfo {
    QString company;
    QString policyNumber;
    QString claimNumber;
    QString dateOfLoss;

    bool isEmpty() const {
        return company.isEmpty() && policyNumber.isEmpty()
            && claimNumber.isEmpty() && dateOfLoss.isEmpty();
    }
};

// The insurance reports. Both throw std::runtime_error if the PDF cannot be
// written. A single photo or receipt that cannot be read does not stop the
// report; it prints as a named placeholder. UI thread only (Qt PDF).
namespace Report {

void generate(const QVector<AssetRecord> &records, const QString &dest,
              const InsurerInfo &insurer);

void generateSingle(const Asset &asset, const QVector<AssetFile> &files, const QString &dest,
                    const InsurerInfo &insurer);

} // namespace Report
