"""Builds the plain-language guide to setting up online sync for Asset Tracker.

    python make_sync_setup_guide.py [output-dir]

Writes Asset-Tracker-Online-Sync-Setup-<VERSION>.pdf. Versions are yyyy.nnn: the
year, then a release number that starts at 001 each year. Bump VERSION and
RELEASED for every change, and keep older PDFs; never overwrite a released one.

Needs reportlab and the DejaVu Sans fonts (for arrows and other symbols that the
built-in PDF fonts lack).
"""
import pathlib
import sys

from reportlab.lib import colors
from reportlab.lib.enums import TA_CENTER
from reportlab.lib.pagesizes import LETTER
from reportlab.lib.styles import ParagraphStyle
from reportlab.lib.units import inch
from reportlab.pdfbase import pdfmetrics
from reportlab.pdfbase.ttfonts import TTFont
from reportlab.platypus import (KeepTogether, PageBreak, Paragraph, Preformatted,
                                SimpleDocTemplate, Spacer, Table, TableStyle)

VERSION = "2026.001"
RELEASED = "November 2026"
TITLE = "Setting Up Online Sync"

HERE = pathlib.Path(__file__).resolve().parent
OUT_DIR = pathlib.Path(sys.argv[1]) if len(sys.argv) > 1 else HERE.parent

# ── Fonts ─────────────────────────────────────────────────────────────────────
FONT_DIRS = [pathlib.Path("/usr/share/fonts/truetype/dejavu"),
             pathlib.Path("C:/Windows/Fonts"), HERE / "fonts"]


def register(name, filename):
    for folder in FONT_DIRS:
        if (folder / filename).exists():
            pdfmetrics.registerFont(TTFont(name, str(folder / filename)))
            return
    sys.exit(f"Font {filename} not found in {', '.join(map(str, FONT_DIRS))}")


register("Body", "DejaVuSans.ttf")
register("Body-Bold", "DejaVuSans-Bold.ttf")
register("Mono", "DejaVuSansMono.ttf")
pdfmetrics.registerFontFamily("Body", normal="Body", bold="Body-Bold",
                              italic="Body", boldItalic="Body-Bold")

# ── Look ──────────────────────────────────────────────────────────────────────
NAVY = colors.HexColor("#1B2A3B")
MUTED = colors.HexColor("#5B6675")
RULE = colors.HexColor("#D9DEE5")
SOFT = colors.HexColor("#F4F6F8")
NOTE_BG = colors.HexColor("#EAF1F8")
NOTE_LINE = colors.HexColor("#2F5D8C")
WARN_BG = colors.HexColor("#FDF0EE")
WARN_LINE = colors.HexColor("#B42318")
WRITE_BG = colors.HexColor("#FFF7E0")
WRITE_LINE = colors.HexColor("#B7791F")

BODY = ParagraphStyle("body", fontName="Body", fontSize=11, leading=16, textColor=colors.black,
                      spaceAfter=6)
SMALL = ParagraphStyle("small", parent=BODY, fontSize=9.5, leading=13, textColor=MUTED)
H1 = ParagraphStyle("h1", fontName="Body-Bold", fontSize=18, leading=23, textColor=NAVY,
                    spaceBefore=4, spaceAfter=4)
H2 = ParagraphStyle("h2", fontName="Body-Bold", fontSize=12.5, leading=17, textColor=NAVY,
                    spaceBefore=10, spaceAfter=4)
STEP = ParagraphStyle("step", parent=BODY, spaceAfter=0)
CODE = ParagraphStyle("code", fontName="Mono", fontSize=9, leading=12.5)
COVER_TITLE = ParagraphStyle("ct", fontName="Body-Bold", fontSize=30, leading=36,
                             textColor=NAVY, alignment=TA_CENTER)
COVER_SUB = ParagraphStyle("cs", fontName="Body", fontSize=14, leading=20, textColor=MUTED,
                           alignment=TA_CENTER)

CONTENT_W = LETTER[0] - 2 * 0.9 * inch


def p(text, style=BODY):
    return Paragraph(text, style)


def b(text):
    """Bold, for names of buttons and menus exactly as they appear on screen."""
    return f"<b>{text}</b>"


def part(number, title, intro=None):
    items = [PageBreak(), p(f"Part {number}", SMALL), p(title, H1),
             Table([[""]], colWidths=[CONTENT_W], rowHeights=[2],
                   style=[("LINEABOVE", (0, 0), (-1, 0), 1.5, NAVY)]),
             Spacer(1, 8)]
    if intro:
        items.append(p(intro))
    return items


def steps(*texts, start=1):
    """Numbered steps in a two-column table, so wrapped lines stay indented."""
    rows = []
    for n, text in enumerate(texts, start):
        number = Table([[p(f"<b>{n}</b>", ParagraphStyle("n", parent=STEP, textColor=colors.white,
                                                          alignment=TA_CENTER))]],
                       colWidths=[22], rowHeights=[22],
                       style=[("BACKGROUND", (0, 0), (-1, -1), NAVY),
                              ("VALIGN", (0, 0), (-1, -1), "MIDDLE"),
                              ("ROUNDEDCORNERS", [11, 11, 11, 11]),
                              ("TOPPADDING", (0, 0), (-1, -1), 0),
                              ("BOTTOMPADDING", (0, 0), (-1, -1), 2)])
        rows.append([number, p(text, STEP)])
    table = Table(rows, colWidths=[34, CONTENT_W - 34])
    table.setStyle(TableStyle([("VALIGN", (0, 0), (-1, -1), "TOP"),
                               ("LEFTPADDING", (0, 0), (-1, -1), 0),
                               ("TOPPADDING", (0, 0), (-1, -1), 5),
                               ("BOTTOMPADDING", (0, 0), (-1, -1), 5)]))
    return [table, Spacer(1, 6)]


def box(label, text, background, line):
    content = [p(f"<b>{label}</b>", ParagraphStyle("bl", parent=BODY, textColor=line,
                                                    spaceAfter=2))]
    content += [p(t, ParagraphStyle("bt", parent=BODY, spaceAfter=3))
                for t in ([text] if isinstance(text, str) else text)]
    table = Table([[content]], colWidths=[CONTENT_W])
    table.setStyle(TableStyle([("BACKGROUND", (0, 0), (-1, -1), background),
                               ("LINEBEFORE", (0, 0), (0, -1), 4, line),
                               ("LEFTPADDING", (0, 0), (-1, -1), 14),
                               ("RIGHTPADDING", (0, 0), (-1, -1), 12),
                               ("TOPPADDING", (0, 0), (-1, -1), 9),
                               ("BOTTOMPADDING", (0, 0), (-1, -1), 6)]))
    return [KeepTogether([table]), Spacer(1, 8)]


def note(text):
    return box("Good to know", text, NOTE_BG, NOTE_LINE)


def warning(text):
    return box("Important", text, WARN_BG, WARN_LINE)


def write_down(text):
    return box("Write this down", text, WRITE_BG, WRITE_LINE)


def code_box(text):
    table = Table([[Preformatted(text, CODE)]], colWidths=[CONTENT_W])
    table.setStyle(TableStyle([("BACKGROUND", (0, 0), (-1, -1), SOFT),
                               ("BOX", (0, 0), (-1, -1), 0.75, RULE),
                               ("LEFTPADDING", (0, 0), (-1, -1), 12),
                               ("TOPPADDING", (0, 0), (-1, -1), 10),
                               ("BOTTOMPADDING", (0, 0), (-1, -1), 10)]))
    return [KeepTogether([table]), Spacer(1, 8)]


def record_sheet():
    rows = [
        ("Google account email", "The Gmail address you use for Firebase"),
        ("Project ID", "Part 1, step 4"),
        ("App login email", "Part 3, step 7"),
        ("App login password", "Part 3, step 7"),
        ("API key", "Part 6, step 7"),
        ("Storage bucket", "Part 6, step 7"),
        ("Sync password", "Part 7, step 6"),
    ]
    data = [[p(f"<b>{name}</b>"), p(where, SMALL), ""] for name, where in rows]
    table = Table([[p("<b>Item</b>", SMALL), p("<b>Where it comes from</b>", SMALL),
                    p("<b>Write it here</b>", SMALL)]] + data,
                  colWidths=[1.7 * inch, 1.75 * inch, CONTENT_W - 3.45 * inch],
                  rowHeights=[20] + [34] * len(data))
    table.setStyle(TableStyle([("GRID", (0, 0), (-1, -1), 0.6, RULE),
                               ("BACKGROUND", (0, 0), (-1, 0), SOFT),
                               ("VALIGN", (0, 0), (-1, -1), "MIDDLE")]))
    return table


FIRESTORE_RULES = """rules_version = '2';
service cloud.firestore {
  match /databases/{database}/documents {
    match /users/{userId}/{document=**} {
      allow read, write: if request.auth != null
                         && request.auth.uid == userId;
    }
  }
}"""

STORAGE_RULES = """rules_version = '2';
service firebase.storage {
  match /b/{bucket}/o {
    match /users/{userId}/{allPaths=**} {
      allow read, write: if request.auth != null
                         && request.auth.uid == userId;
    }
  }
}"""

ARROW = " → "


def menu(*names):
    return ARROW.join(b(n) for n in names)


# ── Content ───────────────────────────────────────────────────────────────────
def build():
    story = []

    # Cover
    story += [Spacer(1, 1.6 * inch), p("Asset Tracker", COVER_SUB), Spacer(1, 6),
              p(TITLE, COVER_TITLE), Spacer(1, 10),
              p("A step-by-step guide to keeping a safe online copy of your inventory", COVER_SUB),
              Spacer(1, 1.2 * inch)]
    cover = Table([[p(f"<b>Version</b>", SMALL), p(VERSION)],
                   [p("<b>Released</b>", SMALL), p(RELEASED)],
                   [p("<b>Time needed</b>", SMALL), p("About 45 minutes")]],
                  colWidths=[1.4 * inch, 2.2 * inch], hAlign="CENTER")
    cover.setStyle(TableStyle([("LINEBELOW", (0, 0), (-1, -2), 0.6, RULE),
                               ("VALIGN", (0, 0), (-1, -1), "MIDDLE")]))
    story.append(cover)

    # Before you start
    story += [PageBreak(), p("Before you start", H1)]
    story.append(p("Asset Tracker keeps your list of belongings, photos and receipts on your "
                   "computer. Online sync also keeps a copy on the internet, using a free Google "
                   "service called <b>Firebase</b>. If your computer is ever lost, stolen or "
                   "damaged, you can bring everything back on a new one."))
    story.append(p("Before anything leaves your computer, Asset Tracker locks it with a password "
                   "that only you know, called your <b>sync password</b>. Google stores the "
                   "locked copy but cannot open it."))
    story.append(p("What you need", H2))
    story += steps("A computer with Asset Tracker installed.",
                   "A Google account, such as a Gmail address. If you don't have one, create one "
                   "first at <b>accounts.google.com</b>.",
                   "A credit or debit card. Google asks for one to turn on file storage. "
                   "For a home inventory the cost is normally <b>nothing</b>. See Part 2.",
                   "A pen, and the record sheet on the next page.")
    story.append(p("How to follow this guide", H2))
    story.append(p(f"Words in {b('bold')} are the names of buttons, menus and boxes exactly as "
                   f"they appear on screen. An arrow means \"then click\": "
                   f"{menu('Project settings', 'General')} means click Project settings, then "
                   "click General."))
    story += note("Google changes the look of its screens from time to time. If a button has a "
                  "slightly different name than in this guide, click the closest match.")

    # Record sheet
    story += [PageBreak(), p("Your record sheet", H1),
              p("Fill this in as you go. You will need these details again if you set up a "
                "new computer. Keep the sheet somewhere safe, such as with your insurance papers.")]
    story += [record_sheet(), Spacer(1, 14)]
    story += warning(["<b>Never lose your sync password.</b> It cannot be reset or recovered, "
                      "by you, by Uhlix or by Google. Without it, your online copy can never be "
                      "opened again.",
                      "Write it on this sheet, or keep it in a password manager."])

    # Part 1
    story += part(1, "Create your Firebase project",
                  "A \"project\" is your own private space in Firebase. Asset Tracker will store "
                  "your online copy there.")
    story += steps(
        "Open your web browser and go to <b>console.firebase.google.com</b>. Sign in with your "
        "Google account if asked.",
        "Click the button to create a new project. It may say "
        f"{b('Create a project')}, {b('Get started by setting up a Firebase project')} or "
        f"{b('Add project')}.",
        "Type a name for your project, for example <b>My Asset Tracker</b>.",
        "Under the name, Firebase shows a <b>Project ID</b>, such as "
        "<i>my-asset-tracker-1a2b3</i>. Write it on your record sheet.",
        "If asked, tick the box to accept the Firebase terms, then click "
        f"{b('Continue')}.",
        "If asked about <b>AI assistance</b> (Gemini), you can turn it off. Asset Tracker "
        f"doesn't need it. Click {b('Continue')}.",
        "If asked about <b>Google Analytics</b>, turn it off. Asset Tracker doesn't need it.",
        f"Click {b('Create project')}. Wait a minute while Firebase gets it ready, then click "
        f"{b('Continue')}.")
    story.append(p("You are now looking at your project's main page. The menu down the left-hand "
                   "side is used for the rest of this guide."))

    # Part 2
    story += part(2, "Turn on file storage billing",
                  "Google requires a payment card on your project before it will store files "
                  "such as photos. This is called the <b>Blaze</b> plan, which means "
                  "\"pay only for what you use\".")
    story += note(["Google gives every project a free allowance each month. A home inventory with "
                   "photos and receipts normally stays well inside it, so you should not be "
                   "charged.",
                   "Charges only start if you use far more storage than a typical home "
                   "inventory needs."])
    story += steps(
        f"In the menu on the left, near the bottom, click {b('Upgrade')}. You may also see it "
        f"later when you set up storage in Part 5. Either place is fine.",
        f"Choose the {b('Blaze')} plan.",
        "Follow the screens to add your card details. Google calls this a "
        "<b>Cloud Billing account</b>.",
        "If Google offers to set a <b>budget</b>, type a small amount such as <b>1</b> "
        "(one dollar), so that Google emails you if anything is ever charged.",
        "Finish the screens until you return to your project.")
    story += warning("A budget only <b>warns</b> you by email. It does not stop charges. If you "
                     "ever get a budget email, contact whoever supports your Asset Tracker.")

    # Part 3
    story += part(3, "Create your login for Asset Tracker",
                  "Asset Tracker signs in to your project with its own email and password. You "
                  "create that login here. It can be the same email as your Google account, but "
                  "use a different password.")
    story += steps(
        f"In the menu on the left, click {menu('Security', 'Authentication')}. In some "
        f"versions it is under {b('Build')} instead of {b('Security')}.",
        f"Click {b('Get started')}.",
        f"Click the {b('Sign-in method')} tab.",
        f"Click {b('Email/Password')}.",
        f"Turn on the first switch, the one labelled {b('Email/Password')}. Leave the second "
        f"one ({b('Email link')}) off.",
        f"Click {b('Save')}.",
        f"Click the {b('Users')} tab, then {b('Add user')}. Type an email address and a "
        f"password, then click {b('Add user')}. Write both on your record sheet as the "
        "<b>app login</b>.")

    # Part 4
    story += part(4, "Create the database",
                  "The database holds the details of each item: names, values, dates and notes.")
    story += steps(
        f"In the menu on the left, click {menu('Databases & Storage', 'Firestore')}. In some "
        f"versions it is under {b('Build')} and called {b('Firestore Database')}.",
        f"Click {b('Create database')}.",
        f"If asked to choose an edition, choose {b('Standard')}.",
        "Choose a <b>location</b>. Pick one in or near your country. If you're in the United "
        f"States, choose {b('us-central1')}. Click {b('Next')}. The location can't be changed "
        "later.",
        f"Choose {b('Start in production mode')}, then click {b('Create')}.",
        f"When the database is ready, click the {b('Rules')} tab at the top.",
        "Select all the text in the large box and delete it. Then type in exactly the text "
        "below, or copy it from this PDF and paste it in:")
    story += code_box(FIRESTORE_RULES)
    story += steps(f"Click {b('Publish')}.", start=8)
    story += note("These rules mean that only you, when signed in, can read or change your "
                  "data. Nobody else can see it, even if they know your Project ID.")

    # Part 5
    story += part(5, "Create file storage",
                  "File storage holds your photos and receipts.")
    story += steps(
        f"In the menu on the left, click {menu('Databases & Storage', 'Storage')}. In some "
        f"versions it is under {b('Build')}.",
        f"If Firebase asks you to upgrade, follow Part 2 first, then come back here.",
        f"Click {b('Get started')}.",
        f"Choose a <b>location</b>. Choose {b('US-CENTRAL1')}, {b('US-EAST1')} or "
        f"{b('US-WEST1')}. These three include Google's free storage allowance. Other "
        "locations are charged from the first file.",
        f"If asked about security rules, choose {b('Start in production mode')}.",
        f"Click {b('Done')} or {b('Create')}.",
        f"Click the {b('Rules')} tab at the top.",
        "Select all the text in the large box and delete it. Then type in exactly the text "
        "below, or copy it from this PDF and paste it in:")
    story += code_box(STORAGE_RULES)
    story += steps(f"Click {b('Publish')}.", start=9)

    # Part 6
    story += part(6, "Find your project details",
                  "Asset Tracker needs three details to find your project. To get them, you "
                  "register Asset Tracker with your project.")
    story += steps(
        "Click the <b>gear icon</b> next to <b>Project Overview</b> at the top left, then "
        f"{b('Project settings')}.",
        f"On the {b('General')} tab, scroll down to {b('Your apps')}.",
        "Click the <b>Web</b> icon. It looks like this: <b>&lt;/&gt;</b>",
        "Type a nickname, such as <b>Asset Tracker</b>. Leave the Firebase Hosting box "
        f"unticked. Click {b('Register app')}.",
        f"Firebase shows some code. You don't need to do anything with it. Click "
        f"{b('Continue to console')}.",
        f"Back on {b('Project settings')}, scroll down to {b('Your apps')} again. Under "
        f"{b('SDK setup and configuration')}, choose {b('Config')}.",
        "Find these two lines and write the text between the quote marks on your record "
        "sheet:")
    story += code_box('apiKey: "AIzaSy...this long code is your API key..."\n'
                      'storageBucket: "my-asset-tracker-1a2b3.firebasestorage.app"')
    story += write_down(["The <b>API key</b> is a long code starting with <b>AIza</b>. Copy it "
                         "exactly: capital and small letters matter.",
                         "The <b>Storage bucket</b> normally ends in <b>.firebasestorage.app</b>."])

    # Part 7
    story += part(7, "Connect Asset Tracker",
                  "Now you tell Asset Tracker how to reach your project, and choose your sync "
                  "password.")
    story += steps(
        f"Open Asset Tracker. Click {menu('File', 'Sync to Cloud (Firebase)...')}",
        f"On the {b('Firebase')} tab, fill in {b('API key')}, {b('Project ID')} and "
        f"{b('Storage bucket')} from your record sheet. The storage bucket box shows an "
        "example in grey; type over it.",
        f"In {b('Email')} and {b('Password')}, type your <b>app login</b> from Part 3. This is "
        "not your Google password.",
        f"Click the {b('Encryption')} tab.",
        "Choose a <b>sync password</b>. Make it long and memorable, such as four unrelated "
        "words.",
        f"Type it into {b('Sync password')}, and write it on your record sheet <b>now</b>.",
        f"If this is your own computer, tick {b('Remember on this PC')} so you don't have to "
        "type it every time. It is stored locked to your Windows account.",
        f"Click {b('Save Settings')}, then {b('Sync Now')}.")
    story.append(p("The first sync can take a few minutes if you have many photos. When it "
                   "finishes, the window says <b>Sync complete</b>, with the number of items "
                   "and files sent."))
    story += note(f"Run {b('Sync Now')} again whenever you add or change items. Asset Tracker "
                  "does not sync on its own.")

    # Part 8
    story += part(8, "The Android app")
    story.append(p("The current Asset Tracker Android app connects only to the Uhlix Firebase "
                   "project. It cannot yet open a project you have set up yourself with this "
                   "guide."))
    story.append(p("Your online copy is still fully useful without it: it protects your "
                   "inventory, and Part 9 explains how to bring it back on a new computer."))
    story += note("This guide will be updated with Android steps when a version of the app that "
                  "supports your own project is available. Check the version number on the "
                  "cover to see whether you have the latest guide.")

    # Part 9
    story += part(9, "Everyday use and problems")
    story.append(p("Bringing everything back on a new computer", H2))
    story += steps(
        "Install Asset Tracker on the new computer.",
        f"Click {menu('File', 'Sync to Cloud (Firebase)...')} and fill in every box from your "
        "record sheet, including the sync password.",
        f"Click {b('Restore from Cloud...')} and answer {b('Yes')}.",
        "Wait while your items, photos and receipts are downloaded. Anything already on that "
        "computer is kept in a separate folder, not deleted.")
    story.append(p("If something goes wrong", H2))
    problems = [
        ("Authentication failed",
         "The email or password on the Firebase tab is wrong. Use the <b>app login</b> from "
         f"Part 3, not your Google password. To set a new one, go to "
         f"{menu('Authentication', 'Users')} in Firebase, click the three dots next to the "
         f"user, then {b('Reset password')}."),
        ("API key not valid",
         "The API key was copied wrongly. Copy it again from Part 6, step 7."),
        ("Different Sync Password",
         f"The sync password doesn't match your online copy. If you mistyped it, choose "
         f"{b('No')} and type it again. Choose {b('Yes')} only if you have truly lost it: your "
         "computer's copy will be sent again under the new password."),
        ("Storage upload failed",
         "File storage isn't ready. Check that Part 2 (billing) and Part 5 (storage, including "
         "the rules) are done."),
        ("Firestore write failed",
         "The database rules weren't published. Repeat Part 4, steps 6 to 8."),
        ("Assets Only in the Cloud",
         "Your online copy has items that are no longer on this computer. Choose "
         f"{b('Yes')} only if you deleted them on purpose. If you're unsure, choose "
         f"{b('No')}: nothing is lost."),
    ]
    data = [[p("<b>The message says</b>", SMALL), p("<b>What to do</b>", SMALL)]]
    data += [[p(f"<b>{msg}</b>"), p(fix)] for msg, fix in problems]
    table = Table(data, colWidths=[1.75 * inch, CONTENT_W - 1.75 * inch], repeatRows=1)
    table.setStyle(TableStyle([("GRID", (0, 0), (-1, -1), 0.6, RULE),
                               ("BACKGROUND", (0, 0), (-1, 0), SOFT),
                               ("VALIGN", (0, 0), (-1, -1), "TOP"),
                               ("TOPPADDING", (0, 0), (-1, -1), 6),
                               ("BOTTOMPADDING", (0, 0), (-1, -1), 4)]))
    story.append(table)
    return story


def decorate(canvas, doc):
    canvas.saveState()
    canvas.setFont("Body", 8.5)
    canvas.setFillColor(MUTED)
    width, _ = LETTER
    if doc.page > 1:
        canvas.drawString(0.9 * inch, 0.55 * inch, f"Asset Tracker · {TITLE}")
        canvas.drawRightString(width - 0.9 * inch, 0.55 * inch,
                               f"Version {VERSION} · Page {doc.page}")
    canvas.restoreState()


def main():
    OUT_DIR.mkdir(parents=True, exist_ok=True)
    out = OUT_DIR / f"Asset-Tracker-Online-Sync-Setup-{VERSION}.pdf"
    if out.exists() and "--force" not in sys.argv:
        sys.exit(f"{out.name} already exists. Released versions are never overwritten: "
                 "bump VERSION, or pass --force while drafting.")
    doc = SimpleDocTemplate(str(out), pagesize=LETTER, leftMargin=0.9 * inch,
                            rightMargin=0.9 * inch, topMargin=0.85 * inch,
                            bottomMargin=0.85 * inch,
                            title=f"Asset Tracker — {TITLE} ({VERSION})", author="Uhlix",
                            subject="Setting up Firebase for Asset Tracker online sync")
    doc.build(build(), onFirstPage=decorate, onLaterPages=decorate)
    print(out)


if __name__ == "__main__":
    main()
