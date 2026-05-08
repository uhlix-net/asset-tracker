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
    PageBreak, Image, HRFlowable,
)

from .config import APP_NAME
from .models import Asset, AssetFile
from . import storage


_W, _H = LETTER
_MARGIN = 0.85 * inch
_BLUE = colors.HexColor("#1a5276")
_LIGHT_BLUE = colors.HexColor("#d6eaf8")
_GRAY = colors.HexColor("#555555")
_CATEGORY_BG = colors.HexColor("#2e86c1")


def _styles():
    ss = getSampleStyleSheet()
    ss.add(ParagraphStyle("ReportTitle", parent=ss["Title"], fontSize=28,
                          textColor=_BLUE, spaceAfter=12))
    ss.add(ParagraphStyle("ReportSubtitle", parent=ss["Normal"], fontSize=14,
                          textColor=_GRAY, spaceAfter=6))
    ss.add(ParagraphStyle("CategoryHeading", parent=ss["Normal"], fontSize=16,
                          textColor=colors.white, spaceBefore=4, spaceAfter=4,
                          leftIndent=6))
    ss.add(ParagraphStyle("AssetHeading", parent=ss["Heading2"], fontSize=13,
                          textColor=_BLUE, spaceBefore=6, spaceAfter=4))
    ss.add(ParagraphStyle("FieldLabel", parent=ss["Normal"], fontSize=9,
                          textColor=_GRAY))
    ss.add(ParagraphStyle("FieldValue", parent=ss["Normal"], fontSize=10))
    return ss


class _NumberedDoc(SimpleDocTemplate):
    def afterPage(self):
        canvas = self.canv
        canvas.saveState()
        canvas.setFont("Helvetica", 8)
        canvas.setFillColor(_GRAY)
        canvas.drawCentredString(_W / 2, 0.5 * inch, f"Page {canvas.getPageNumber()}")
        canvas.drawString(_MARGIN, 0.5 * inch, APP_NAME)
        canvas.restoreState()


def _title_page(ss, total_assets: int, purchase_total: float | None,
                current_total: float | None) -> list:
    elements = [Spacer(1, 1.5 * inch)]
    elements.append(Paragraph(APP_NAME, ss["ReportTitle"]))
    elements.append(Paragraph("Asset Inventory Report", ss["ReportSubtitle"]))
    elements.append(Spacer(1, 0.2 * inch))
    elements.append(Paragraph(f"Generated: {date.today().strftime('%B %d, %Y')}",
                               ss["FieldLabel"]))
    elements.append(Spacer(1, 0.15 * inch))

    rows = [
        ["Total Assets:", str(total_assets)],
        ["Total Purchase Value:",
         f"${purchase_total:,.2f}" if purchase_total is not None else "—"],
        ["Total Current Value:",
         f"${current_total:,.2f}" if current_total is not None else "—"],
    ]
    tbl = Table(rows, colWidths=[2.5 * inch, 2.5 * inch])
    tbl.setStyle(TableStyle([
        ("FONTNAME", (0, 0), (-1, -1), "Helvetica"),
        ("FONTSIZE", (0, 0), (-1, -1), 11),
        ("FONTNAME", (0, 0), (0, -1), "Helvetica-Bold"),
        ("TEXTCOLOR", (0, 0), (-1, -1), _GRAY),
        ("ROWBACKGROUNDS", (0, 0), (-1, -1), [_LIGHT_BLUE, colors.white]),
        ("GRID", (0, 0), (-1, -1), 0.5, colors.lightgrey),
        ("PADDING", (0, 0), (-1, -1), 6),
    ]))
    elements.append(tbl)
    elements.append(PageBreak())
    return elements


def _toc_page(ss, assets_with_files: list) -> list:
    elements = [Paragraph("Table of Contents", ss["Heading1"]),
                Spacer(1, 0.2 * inch)]

    header = [Paragraph(f"<b>{h}</b>", ss["Normal"]) for h in
              ["ID", "Name", "Category", "Purchase Date", "Purchase Value",
               "Current Value", "Photos"]]
    rows = [header]
    for asset, files in assets_with_files:
        image_count = sum(1 for f in files if f.file_type == "image")
        rows.append([
            Paragraph(asset.id, ss["Normal"]),
            Paragraph(asset.name[:40], ss["Normal"]),
            Paragraph(asset.category or "—", ss["Normal"]),
            Paragraph(asset.date_purchase or "—", ss["Normal"]),
            Paragraph(asset.value_display, ss["Normal"]),
            Paragraph(asset.current_value_display, ss["Normal"]),
            Paragraph(str(image_count), ss["Normal"]),
        ])

    col_widths = [0.6*inch, 2.2*inch, 1.1*inch, 1.0*inch, 1.0*inch, 1.0*inch, 0.5*inch]
    tbl = Table(rows, colWidths=col_widths, repeatRows=1)
    tbl.setStyle(TableStyle([
        ("FONTNAME", (0, 0), (-1, 0), "Helvetica-Bold"),
        ("BACKGROUND", (0, 0), (-1, 0), _BLUE),
        ("TEXTCOLOR", (0, 0), (-1, 0), colors.white),
        ("ROWBACKGROUNDS", (0, 1), (-1, -1), [colors.white, _LIGHT_BLUE]),
        ("GRID", (0, 0), (-1, -1), 0.4, colors.lightgrey),
        ("FONTSIZE", (0, 0), (-1, -1), 8),
        ("PADDING", (0, 0), (-1, -1), 4),
        ("VALIGN", (0, 0), (-1, -1), "MIDDLE"),
    ]))
    elements.append(tbl)
    elements.append(PageBreak())
    return elements


def _category_header(ss, category: str) -> list:
    tbl = Table([[Paragraph(category or "Uncategorized", ss["CategoryHeading"])]],
                colWidths=[_W - 2 * _MARGIN])
    tbl.setStyle(TableStyle([
        ("BACKGROUND", (0, 0), (-1, -1), _CATEGORY_BG),
        ("PADDING", (0, 0), (-1, -1), 6),
    ]))
    return [tbl, Spacer(1, 0.1 * inch)]


def _asset_section(ss, asset: Asset, files: list[AssetFile]) -> list:
    elements = []
    images = [f for f in files if f.file_type == "image"]
    has_receipt = any(f.file_type == "receipt" for f in files)

    elements.append(Paragraph(f"#{asset.id} — {asset.name}", ss["AssetHeading"]))

    meta = [
        ["Asset ID:", asset.id,         "Serial Number:", asset.serial_number or "—"],
        ["Name:",     asset.name,        "Model Number:",  asset.model_number or "—"],
        ["Category:", asset.category or "—", "Purchase Date:", asset.date_purchase or "—"],
        ["Purchase Value:", asset.value_display,
         "Current Value:", asset.current_value_display],
        ["Receipt:", "Yes ✓" if has_receipt else "No",
         "Date Added:", asset.date_added[:10]],
    ]
    if asset.notes:
        meta.append(["Notes:", asset.notes, "", ""])

    col_widths = [1.1*inch, 2.2*inch, 1.2*inch, 2.0*inch]
    meta_tbl = Table(meta, colWidths=col_widths)
    meta_tbl.setStyle(TableStyle([
        ("FONTNAME", (0, 0), (0, -1), "Helvetica-Bold"),
        ("FONTNAME", (2, 0), (2, -1), "Helvetica-Bold"),
        ("FONTSIZE", (0, 0), (-1, -1), 9),
        ("TEXTCOLOR", (0, 0), (0, -1), _GRAY),
        ("TEXTCOLOR", (2, 0), (2, -1), _GRAY),
        ("BACKGROUND", (0, 0), (-1, -1), _LIGHT_BLUE),
        ("GRID", (0, 0), (-1, -1), 0.3, colors.lightgrey),
        ("PADDING", (0, 0), (-1, -1), 5),
    ]))
    elements.append(meta_tbl)
    elements.append(Spacer(1, 0.12 * inch))

    if images:
        elements.append(Paragraph("Photos:", ss["FieldLabel"]))
        elements.append(Spacer(1, 0.05 * inch))
        img_size = 2.4 * inch
        row_cells, photo_rows = [], []
        for af in images:
            path = storage.get_stored_path(asset, af)
            try:
                img = Image(str(path), width=img_size, height=img_size, kind="proportional")
                row_cells.append(img)
            except Exception:
                row_cells.append(Paragraph(f"[{af.file_name}]", ss["FieldLabel"]))
            if len(row_cells) == 2:
                photo_rows.append(row_cells)
                row_cells = []
        if row_cells:
            row_cells += [""] * (2 - len(row_cells))
            photo_rows.append(row_cells)
        if photo_rows:
            photo_tbl = Table(photo_rows,
                              colWidths=[img_size + 0.2*inch, img_size + 0.2*inch])
            photo_tbl.setStyle(TableStyle([
                ("ALIGN", (0, 0), (-1, -1), "CENTER"),
                ("VALIGN", (0, 0), (-1, -1), "MIDDLE"),
                ("GRID", (0, 0), (-1, -1), 0.3, colors.lightgrey),
                ("PADDING", (0, 0), (-1, -1), 4),
            ]))
            elements.append(photo_tbl)
    else:
        elements.append(Paragraph("No photos on file.", ss["FieldLabel"]))

    elements.append(Spacer(1, 0.25 * inch))
    elements.append(HRFlowable(width="100%", thickness=0.5, color=colors.lightgrey))
    elements.append(Spacer(1, 0.15 * inch))
    return elements


def generate_report(
    assets_with_files: list[tuple[Asset, list[AssetFile]]],
    output_path: str | pathlib.Path,
) -> None:
    ss = _styles()
    doc = _NumberedDoc(
        str(output_path),
        pagesize=LETTER,
        leftMargin=_MARGIN, rightMargin=_MARGIN,
        topMargin=_MARGIN, bottomMargin=_MARGIN,
    )

    purchase_total: float | None = None
    current_total: float | None = None
    for asset, _ in assets_with_files:
        if asset.value_estimate is not None:
            purchase_total = (purchase_total or 0) + asset.value_estimate
        if asset.current_value is not None:
            current_total = (current_total or 0) + asset.current_value

    story = []
    story += _title_page(ss, len(assets_with_files), purchase_total, current_total)
    story += _toc_page(ss, assets_with_files)

    # Group by category for report body
    sorted_data = sorted(assets_with_files, key=lambda x: (x[0].category or "zzz", x[0].id))
    for category, group in groupby(sorted_data, key=lambda x: x[0].category):
        story += _category_header(ss, category)
        for asset, files in group:
            story += _asset_section(ss, asset, files)
        story.append(PageBreak())

    doc.build(story)
