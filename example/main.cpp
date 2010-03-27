#include "qapttest.h"
#include <kapplication.h>
#include <kaboutdata.h>
#include <kcmdlineargs.h>
#include <KDE/KLocale>

static const char description[] =
    I18N_NOOP("A KDE 4 Application");

static const char version[] = "0.1";

int main(int argc, char **argv)
{
    KAboutData about("qapttest", 0, ki18n("LibQApt test"), version, ki18n(description),
                     KAboutData::License_GPL, ki18n("(C) 2007 Jonathan Thomas"), KLocalizedString(), 0, "echidnaman@kubuntu.org");
    about.addAuthor( ki18n("Jonathan Thomas"), KLocalizedString(), "echidnaman@kubuntu.org" );
    KCmdLineArgs::init(argc, argv, &about);

    KApplication app;

    qapttest *widget = new qapttest;

    widget->show();
    return app.exec();
}
