from __future__ import annotations
import pathlib
from datetime import date
from itertools import groupby

from reportlab.lib import colors
from reportlab.lib.pagesizes import LETTER
from reportlab.lib.styles import getSampleStyleSheet, ParagraphStyle
from reportlab.lib.units import inch
from reportlab.platypus import (
    SimpleDocTemplate, Paragraph, Spacer, Table, TableStyle,
    PageBreak, Image, HRFlowable, KeepTogether,
)

from .config import APP_NAME
from .models import Asset, AssetFile
from . import storage

# ── Document metrics ──────────────────────────────────────────────────────────
_W, _H = LETTER
_MARGIN = 1.0 * inch
_CONTENT_W = _W - 2 * _MARGIN

# ── Colour palette (formal / legal) ──────────────────────────────────────────
_NAVY       = colors.HexColor("#1B2A3B")   # headings
_NAVY_MID   = colors.HexColor("#2C3E50")   # sub-headings
_SLATE      = colors.HexColor("#4A5568")   # labels / secondary text
_RULE       = colors.HexColor("#8896A6")   # horizontal rules
_LIGHT_BG   = colors.HexColor("#F4F6F8")   # table row shading
_WHITE      = colors.white
_BLACK      = colors.black


# ── Style sheet ───────────────────────────────────────────────────────────────
def _styles():
    ss = getSampleStyleSheet()

    ss.add(ParagraphStyle("DocTitle", parent=ss["Title"],
        fontSize=22, textColor=_NAVY, leading=28,
        spaceAfter=4, alignment=1))                        # centred

    ss.add(ParagraphStyle("DocSubtitle", parent=ss["Normal"],
        fontSize=11, textColor=_SLATE, leading=15,
        spaceAfter=2, alignment=1))

    ss.add(ParagraphStyle("SectionHead", parent=ss["Normal"],
        fontSize=9, textColor=_SLATE, leading=12,
        spaceBefore=4, spaceAfter=2,
        fontName="Helvetica-Bold", letterSpacing=1.2))

    ss.add(ParagraphStyle("Declaration", parent=ss["Normal"],
        fontSize=9, textColor=_BLACK, leading=14,
        spaceAfter=6))

    ss.add(ParagraphStyle("CatHeading", parent=ss["Normal"],
        fontSize=13, textColor=_WHITE, leading=17,
        fontName="Helvetica-Bold", leftIndent=6))

    ss.add(ParagraphStyle("AssetID", parent=ss["Normal"],
        fontSize=10, textColor=_SLATE, leading=14,
        fontName="Helvetica-Bold"))

    ss.add(ParagraphStyle("AssetName", parent=ss["Normal"],
        fontSize=13, textColor=_NAVY, leading=17,
        fontName="Helvetica-Bold", spaceAfter=2))

    ss.add(ParagraphStyle("FieldLabel", parent=ss["Normal"],
        fontSize=8, textColor=_SLATE, leading=11,
        fontName="Helvetica-Bold"))

    ss.add(ParagraphStyle("FieldValue", parent=ss["Normal"],
        fontSize=9, textColor=_BLACK, leading=12))

    ss.add(ParagraphStyle("Caption", parent=ss["Normal"],
        fontSize=7, textColor=_SLATE, leading=9, alignment=1))

    ss.add(ParagraphStyle("FootNote", parent=ss["Normal"],
        fontSize=7, textColor=_SLATE, leading=10, alignment=1))

    ss.add(ParagraphStyle("TOCHead", parent=ss["Normal"],
        fontSize=14, textColor=_NAVY, leading=18,
        fontName="Helvetica-Bold", spaceAfter=8))

    return ss


# ── Document template with running header / footer ────────────────────────────
class _LegalDoc(SimpleDocTemplate):
    """Adds a formal header + footer to every page after the cover."""

    def __init__(self, *args, **kwargs):
        self._doc_date = kwargs.pop("doc_date", "")
        super().__init__(*args, **kwargs)

    def afterPage(self):
        c = self.canv
        page = c.getPageNumber()

        c.saveState()
        c.setFont("Helvetica", 7.5)
        c.setFillColor(_SLATE)

        if page > 1:
            # ── Running header ────────────────────────────────────────────────
            c.drawString(_MARGIN, _H - _MARGIN * 0.55,
                         "PERSONAL PROPERTY INVENTORY REPORT")
            c.drawRightString(_W - _MARGIN, _H - _MARGIN * 0.55,
                              f"Prepared: {self._doc_date}")
            c.setStrokeColor(_RULE)
            c.setLineWidth(0.5)
            c.line(_MARGIN, _H - _MARGIN * 0.6,
                   _W - _MARGIN, _H - _MARGIN * 0.6)

        # ── Footer (all pages) ────────────────────────────────────────────────
        c.setStrokeColor(_RULE)
        c.setLineWidth(0.5)
        c.line(_MARGIN, 0.55 * inch, _W - _MARGIN, 0.55 * inch)
        c.drawString(_MARGIN, 0.38 * inch,
                     "CONFIDENTIAL — For Insurance Claim Purposes Only")
        c.drawCentredString(_W / 2, 0.38 * inch, f"Page {page}")
        c.drawRightString(_W - _MARGIN, 0.38 * inch, self._doc_date)

        c.restoreState()


# ── Helpers ───────────────────────────────────────────────────────────────────
def _rule(width="100%", color=_RULE, thickness=0.5):
    return HRFlowable(width=width, thickness=thickness, color=color,
                      spaceAfter=4, spaceBefore=4)


def _sig_table(label: str) -> Table:
    """Returns a signature-line row."""
    tbl = Table(
        [[Paragraph(label, ParagraphStyle("sl", fontSize=8,
                                          textColor=_SLATE,
                                          fontName="Helvetica")),
          ""]],
        colWidths=[1.5 * inch, 3.5 * inch],
    )
    tbl.setStyle(TableStyle([
        ("LINEBELOW", (1, 0), (1, 0), 0.75, _BLACK),
        ("VALIGN", (0, 0), (-1, -1), "BOTTOM"),
        ("BOTTOMPADDING", (0, 0), (-1, -1), 2),
        ("FONTSIZE", (0, 0), (-1, -1), 8),
    ]))
    return tbl


# ── Title / cover page ────────────────────────────────────────────────────────
def _title_page(ss, total_assets: int,
                purchase_total: float | None,
                current_total: float | None,
                doc_date: str) -> list:
    els = []
    els.append(Spacer(1, 1.2 * inch))

    # Title block
    els.append(HRFlowable(width="100%", thickness=1.5,
                           color=_NAVY, spaceAfter=10))
    els.append(Paragraph("PERSONAL PROPERTY INVENTORY REPORT", ss["DocTitle"]))
    els.append(Paragraph("Prepared for Insurance Claim Purposes", ss["DocSubtitle"]))
    els.append(HRFlowable(width="100%", thickness=1.5,
                           color=_NAVY, spaceBefore=10, spaceAfter=20))

    # Summary table
    rows = [
        ["Date of Preparation:", doc_date],
        ["Total Items Inventoried:", str(total_assets)],
        ["Total Purchase Price:",
         f"${purchase_total:,.2f}" if purchase_total is not None else "Not recorded"],
        ["Total Current Value:",
         f"${current_total:,.2f}" if current_total is not None else "Not recorded"],
    ]
    tbl = Table(rows, colWidths=[2.6 * inch, 3.5 * inch])
    tbl.setStyle(TableStyle([
        ("FONTNAME", (0, 0), (0, -1), "Helvetica-Bold"),
        ("FONTNAME", (1, 0), (1, -1), "Helvetica"),
        ("FONTSIZE", (0, 0), (-1, -1), 10),
        ("TEXTCOLOR", (0, 0), (0, -1), _SLATE),
        ("TEXTCOLOR", (1, 0), (1, -1), _BLACK),
        ("ROWBACKGROUNDS", (0, 0), (-1, -1), [_LIGHT_BG, _WHITE]),
        ("GRID", (0, 0), (-1, -1), 0.4, _RULE),
        ("PADDING", (0, 0), (-1, -1), 7),
    ]))
    els.append(tbl)
    els.append(Spacer(1, 0.3 * inch))

    # Declaration
    els.append(_rule(color=_NAVY_MID))
    els.append(Spacer(1, 0.1 * inch))
    els.append(Paragraph("DECLARATION", ss["SectionHead"]))
    els.append(Spacer(1, 0.05 * inch))
    els.append(Paragraph(
        "I hereby certify that this inventory represents a true and accurate accounting "
        "of the personal property described herein, prepared in good faith for the purposes "
        "of filing an insurance claim. All monetary values listed are estimates or documented "
        "purchase prices unless otherwise noted. Photographic evidence and supporting receipts, "
        "where available, are included in this report.",
        ss["Declaration"],
    ))
    els.append(Spacer(1, 0.25 * inch))

    # Signature block
    els.append(_sig_table("Owner / Claimant:"))
    els.append(Spacer(1, 0.2 * inch))
    els.append(_sig_table("Date:"))
    els.append(Spacer(1, 0.35 * inch))

    # Disclaimer
    els.append(_rule())
    els.append(Spacer(1, 0.05 * inch))
    els.append(Paragraph(
        f"This document was generated by {APP_NAME} software on {doc_date}. "
        "The accuracy of the information contained herein is the sole responsibility "
        "of the preparer. This report does not constitute a legal appraisal.",
        ss["FootNote"],
    ))

    els.append(PageBreak())
    return els


# ── Table of contents ─────────────────────────────────────────────────────────
def _toc_page(ss, assets_with_files: list) -> list:
    els = [Paragraph("TABLE OF CONTENTS", ss["TOCHead"]), _rule(color=_NAVY)]

    header = [
        Paragraph("<b>Asset ID</b>", ss["FieldLabel"]),
        Paragraph("<b>Description</b>", ss["FieldLabel"]),
        Paragraph("<b>Category</b>", ss["FieldLabel"]),
        Paragraph("<b>Purchase Date</b>", ss["FieldLabel"]),
        Paragraph("<b>Purchase Price</b>", ss["FieldLabel"]),
        Paragraph("<b>Current Value</b>", ss["FieldLabel"]),
        Paragraph("<b>Photos</b>", ss["FieldLabel"]),
    ]
    rows = [header]

    for asset, files in assets_with_files:
        img_count = sum(1 for f in files if f.file_type == "image")
        rows.append([
            Paragraph(asset.id, ss["FieldValue"]),
            Paragraph(asset.name[:42], ss["FieldValue"]),
            Paragraph(asset.category or "—", ss["FieldValue"]),
            Paragraph(asset.date_purchase or "—", ss["FieldValue"]),
            Paragraph(asset.value_display, ss["FieldValue"]),
            Paragraph(asset.current_value_display, ss["FieldValue"]),
            Paragraph(str(img_count), ss["FieldValue"]),
        ])

    col_w = [0.65*inch, 2.2*inch, 1.05*inch, 0.9*inch, 0.9*inch, 0.9*inch, 0.5*inch]
    tbl = Table(rows, colWidths=col_w, repeatRows=1)
    tbl.setStyle(TableStyle([
        ("BACKGROUND", (0, 0), (-1, 0), _NAVY),
        ("TEXTCOLOR", (0, 0), (-1, 0), _WHITE),
        ("FONTNAME", (0, 0), (-1, 0), "Helvetica-Bold"),
        ("ROWBACKGROUNDS", (0, 1), (-1, -1), [_WHITE, _LIGHT_BG]),
        ("GRID", (0, 0), (-1, -1), 0.3, _RULE),
        ("FONTSIZE", (0, 0), (-1, -1), 8),
        ("PADDING", (0, 0), (-1, -1), 4),
        ("VALIGN", (0, 0), (-1, -1), "MIDDLE"),
        ("ALIGN", (4, 0), (5, -1), "RIGHT"),
    ]))
    els.append(tbl)
    els.append(PageBreak())
    return els


# ── Category divider page ─────────────────────────────────────────────────────
def _category_divider(ss, category: str, count: int) -> list:
    label = category if category else "Uncategorized"
    tbl = Table(
        [[Paragraph(label.upper(), ss["CatHeading"]),
          Paragraph(f"{count} item{'s' if count != 1 else ''}",
                    ParagraphStyle("cnt", fontSize=9, textColor=_WHITE,
                                   alignment=2))]],
        colWidths=[_CONTENT_W * 0.75, _CONTENT_W * 0.25],
    )
    tbl.setStyle(TableStyle([
        ("BACKGROUND", (0, 0), (-1, -1), _NAVY),
        ("PADDING", (0, 0), (-1, -1), 8),
        ("VALIGN", (0, 0), (-1, -1), "MIDDLE"),
    ]))
    return [tbl, Spacer(1, 0.2 * inch)]


# ── Single asset record ───────────────────────────────────────────────────────
def _asset_record(ss, asset: Asset, files: list[AssetFile]) -> list:
    images  = [f for f in files if f.file_type == "image"]
    receipt = next((f for f in files if f.file_type == "receipt"), None)

    block = []

    # ── Header row ────────────────────────────────────────────────────────────
    hdr = Table(
        [[Paragraph(asset.name, ss["AssetName"]),
          Paragraph(f"Asset ID: {asset.id}", ss["AssetID"])]],
        colWidths=[_CONTENT_W * 0.72, _CONTENT_W * 0.28],
    )
    hdr.setStyle(TableStyle([
        ("VALIGN", (0, 0), (-1, -1), "BOTTOM"),
        ("ALIGN", (1, 0), (1, 0), "RIGHT"),
        ("LINEBELOW", (0, 0), (-1, 0), 0.75, _NAVY),
        ("BOTTOMPADDING", (0, 0), (-1, -1), 4),
    ]))
    block.append(hdr)
    block.append(Spacer(1, 0.08 * inch))

    # ── Two-column detail grid ─────────────────────────────────────────────────
    def field(label, value):
        return [
            Paragraph(label, ss["FieldLabel"]),
            Paragraph(str(value) if value else "—", ss["FieldValue"]),
        ]

    left_col = [
        field("CATEGORY",       asset.category or "—"),
        field("SERIAL NUMBER",  asset.serial_number or "—"),
        field("MODEL NUMBER",   asset.model_number or "—"),
        field("DATE ADDED",     asset.date_added[:10]),
    ]
    right_col = [
        field("PURCHASE DATE",  asset.date_purchase or "—"),
        field("PURCHASE PRICE", asset.value_display),
        field("CURRENT VALUE",  asset.current_value_display),
        field("RECEIPT ON FILE","Yes" if asset.has_receipt else "No"),
    ]

    def mini_table(rows):
        t = Table(rows, colWidths=[1.15 * inch, 1.85 * inch])
        t.setStyle(TableStyle([
            ("FONTSIZE", (0, 0), (-1, -1), 8),
            ("PADDING", (0, 0), (-1, -1), 3),
            ("ROWBACKGROUNDS", (0, 0), (-1, -1), [_LIGHT_BG, _WHITE]),
            ("GRID", (0, 0), (-1, -1), 0.3, _RULE),
            ("VALIGN", (0, 0), (-1, -1), "TOP"),
        ]))
        return t

    detail = Table(
        [[mini_table(left_col), mini_table(right_col)]],
        colWidths=[3.15 * inch, 3.15 * inch],
    )
    detail.setStyle(TableStyle([
        ("VALIGN", (0, 0), (-1, -1), "TOP"),
        ("LEFTPADDING",  (0, 0), (0, 0), 0),
        ("RIGHTPADDING", (0, 0), (0, 0), 6),
        ("LEFTPADDING",  (1, 0), (1, 0), 6),
        ("RIGHTPADDING", (1, 0), (1, 0), 0),
    ]))
    block.append(detail)

    # ── Notes ─────────────────────────────────────────────────────────────────
    if asset.notes and asset.notes.strip():
        block.append(Spacer(1, 0.06 * inch))
        notes_tbl = Table(
            [[Paragraph("NOTES", ss["FieldLabel"]),
              Paragraph(asset.notes, ss["FieldValue"])]],
            colWidths=[0.9 * inch, _CONTENT_W - 0.9 * inch],
        )
        notes_tbl.setStyle(TableStyle([
            ("BACKGROUND", (0, 0), (-1, -1), _LIGHT_BG),
            ("GRID", (0, 0), (-1, -1), 0.3, _RULE),
            ("PADDING", (0, 0), (-1, -1), 4),
            ("VALIGN", (0, 0), (-1, -1), "TOP"),
        ]))
        block.append(notes_tbl)

    # ── Photographs ───────────────────────────────────────────────────────────
    if images:
        block.append(Spacer(1, 0.1 * inch))
        block.append(Paragraph("PHOTOGRAPHIC DOCUMENTATION", ss["SectionHead"]))
        block.append(Spacer(1, 0.04 * inch))

        img_size = 2.2 * inch
        row_cells, photo_rows = [], []

        for af in images:
            path = storage.get_stored_path(asset, af)
            try:
                cell_content = [
                    Image(str(path), width=img_size, height=img_size,
                          kind="proportional"),
                    Paragraph(af.file_name, ss["Caption"]),
                ]
                cell = Table([[c] for c in cell_content],
                              colWidths=[img_size + 0.1 * inch])
                cell.setStyle(TableStyle([
                    ("ALIGN", (0, 0), (-1, -1), "CENTER"),
                    ("PADDING", (0, 0), (-1, -1), 2),
                ]))
                row_cells.append(cell)
            except Exception:
                row_cells.append(Paragraph(f"[{af.file_name}]", ss["FieldLabel"]))

            if len(row_cells) == 3:
                photo_rows.append(row_cells[:])
                row_cells = []

        if row_cells:
            while len(row_cells) < 3:
                row_cells.append("")
            photo_rows.append(row_cells)

        if photo_rows:
            cw = (img_size + 0.15 * inch)
            photo_tbl = Table(photo_rows,
                              colWidths=[cw, cw, cw])
            photo_tbl.setStyle(TableStyle([
                ("ALIGN", (0, 0), (-1, -1), "CENTER"),
                ("VALIGN", (0, 0), (-1, -1), "TOP"),
                ("GRID", (0, 0), (-1, -1), 0.3, _RULE),
                ("PADDING", (0, 0), (-1, -1), 4),
            ]))
            block.append(photo_tbl)

    # ── Record separator ──────────────────────────────────────────────────────
    block.append(Spacer(1, 0.14 * inch))
    block.append(_rule(color=_RULE, thickness=0.75))
    block.append(Spacer(1, 0.1 * inch))

    return [KeepTogether(block[:4])] + block[4:]


# ── Public entry point ────────────────────────────────────────────────────────
def generate_report(
    assets_with_files: list[tuple[Asset, list[AssetFile]]],
    output_path: str | pathlib.Path,
) -> None:
    today = date.today().strftime("%B %d, %Y")
    output_path = str(output_path)

    doc = _LegalDoc(
        output_path,
        pagesize=LETTER,
        leftMargin=_MARGIN,
        rightMargin=_MARGIN,
        topMargin=_MARGIN,
        bottomMargin=0.85 * inch,
        doc_date=today,
    )

    ss = _styles()

    purchase_total: float | None = None
    current_total:  float | None = None
    for asset, _ in assets_with_files:
        if asset.value_estimate is not None:
            purchase_total = (purchase_total or 0) + asset.value_estimate
        if asset.current_value is not None:
            current_total = (current_total or 0) + asset.current_value

    story = []
    story += _title_page(ss, len(assets_with_files),
                         purchase_total, current_total, today)
    story += _toc_page(ss, assets_with_files)

    # Group by category; page break between categories
    sorted_data = sorted(assets_with_files,
                         key=lambda x: (x[0].category or "zzz", x[0].id))
    first_cat = True
    for category, group in groupby(sorted_data,
                                   key=lambda x: x[0].category):
        items = list(group)
        if not first_cat:
            story.append(PageBreak())
        first_cat = False
        story += _category_divider(ss, category, len(items))
        for asset, files in items:
            story += _asset_record(ss, asset, files)

    doc.build(story)
