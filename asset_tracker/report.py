from __future__ import annotations
import pathlib
from datetime import date

from reportlab.lib import colors
from reportlab.lib.pagesizes import LETTER
from reportlab.lib.styles import getSampleStyleSheet, ParagraphStyle
from reportlab.lib.units import inch
from reportlab.platypus import (
    SimpleDocTemplate, Paragraph, Spacer, Table, TableStyle,
    PageBreak, Image, KeepTogether,
)
from reportlab.platypus.tableofcontents import TableOfContents

from .config import APP_NAME, ASSETS_DIR
from .models import Asset, AssetFile
from . import storage


_W, _H = LETTER
_MARGIN = 0.85 * inch

_BLUE = colors.HexColor("#1a5276")
_LIGHT_BLUE = colors.HexColor("#d6eaf8")
_GRAY = colors.HexColor("#555555")


def _styles():
    ss = getSampleStyleSheet()
    ss.add(ParagraphStyle(
        "ReportTitle",
        parent=ss["Title"],
        fontSize=28,
        textColor=_BLUE,
        spaceAfter=12,
    ))
    ss.add(ParagraphStyle(
        "ReportSubtitle",
        parent=ss["Normal"],
        fontSize=14,
        textColor=_GRAY,
        spaceAfter=6,
    ))
    ss.add(ParagraphStyle(
        "AssetHeading",
        parent=ss["Heading2"],
        fontSize=14,
        textColor=_BLUE,
        spaceBefore=6,
        spaceAfter=4,
        borderPad=4,
    ))
    ss.add(ParagraphStyle(
        "FieldLabel",
        parent=ss["Normal"],
        fontSize=9,
        textColor=_GRAY,
    ))
    ss.add(ParagraphStyle(
        "FieldValue",
        parent=ss["Normal"],
        fontSize=10,
    ))
    ss.add(ParagraphStyle(
        "TOCEntry",
        parent=ss["Normal"],
        fontSize=10,
        leftIndent=20,
    ))
    return ss


class _NumberedDoc(SimpleDocTemplate):
    """SimpleDocTemplate that prints page numbers in the footer."""

    def handle_pageBegin(self):
        super().handle_pageBegin()

    def afterPage(self):
        canvas = self.canv
        canvas.saveState()
        canvas.setFont("Helvetica", 8)
        canvas.setFillColor(_GRAY)
        canvas.drawCentredString(_W / 2, 0.5 * inch, f"Page {canvas.getPageNumber()}")
        canvas.drawString(_MARGIN, 0.5 * inch, APP_NAME)
        canvas.restoreState()


def _title_page(ss, total_assets: int, total_value: float | None) -> list:
    elements = []
    elements.append(Spacer(1, 1.5 * inch))
    elements.append(Paragraph(APP_NAME, ss["ReportTitle"]))
    elements.append(Paragraph("Asset Inventory Report", ss["ReportSubtitle"]))
    elements.append(Spacer(1, 0.2 * inch))

    today = date.today().strftime("%B %d, %Y")
    elements.append(Paragraph(f"Generated: {today}", ss["FieldLabel"]))
    elements.append(Spacer(1, 0.15 * inch))

    summary_data = [
        ["Total Assets:", str(total_assets)],
        [
            "Total Estimated Value:",
            f"${total_value:,.2f}" if total_value is not None else "—",
        ],
    ]
    tbl = Table(summary_data, colWidths=[2.5 * inch, 2.5 * inch])
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
    elements = []
    elements.append(Paragraph("Table of Contents", ss["Heading1"]))
    elements.append(Spacer(1, 0.2 * inch))

    header = [
        Paragraph("<b>ID</b>", ss["Normal"]),
        Paragraph("<b>Name</b>", ss["Normal"]),
        Paragraph("<b>Purchase Date</b>", ss["Normal"]),
        Paragraph("<b>Est. Value</b>", ss["Normal"]),
        Paragraph("<b>Photos</b>", ss["Normal"]),
    ]
    rows = [header]
    for asset, files in assets_with_files:
        image_count = sum(1 for f in files if f.file_type == "image")
        rows.append([
            Paragraph(asset.id, ss["Normal"]),
            Paragraph(asset.name[:50], ss["Normal"]),
            Paragraph(asset.date_purchase or "—", ss["Normal"]),
            Paragraph(asset.value_display, ss["Normal"]),
            Paragraph(str(image_count), ss["Normal"]),
        ])

    col_widths = [0.7 * inch, 3.0 * inch, 1.2 * inch, 1.2 * inch, 0.7 * inch]
    tbl = Table(rows, colWidths=col_widths, repeatRows=1)
    tbl.setStyle(TableStyle([
        ("FONTNAME", (0, 0), (-1, 0), "Helvetica-Bold"),
        ("BACKGROUND", (0, 0), (-1, 0), _BLUE),
        ("TEXTCOLOR", (0, 0), (-1, 0), colors.white),
        ("ROWBACKGROUNDS", (0, 1), (-1, -1), [colors.white, _LIGHT_BLUE]),
        ("GRID", (0, 0), (-1, -1), 0.4, colors.lightgrey),
        ("FONTSIZE", (0, 0), (-1, -1), 9),
        ("PADDING", (0, 0), (-1, -1), 5),
        ("VALIGN", (0, 0), (-1, -1), "MIDDLE"),
    ]))
    elements.append(tbl)
    elements.append(PageBreak())
    return elements


def _asset_page(ss, asset: Asset, files: list[AssetFile]) -> list:
    elements = []
    images = [f for f in files if f.file_type == "image"]
    has_receipt = any(f.file_type == "receipt" for f in files)

    # Section heading
    elements.append(
        Paragraph(f"Asset #{asset.id} — {asset.name}", ss["AssetHeading"])
    )

    # Metadata table
    meta = [
        ["Asset ID:", asset.id, "Purchase Date:", asset.date_purchase or "—"],
        ["Name:", asset.name, "Est. Value:", asset.value_display],
        ["Date Added:", asset.date_added[:10], "Receipt:", "Yes ✓" if has_receipt else "No"],
    ]
    if asset.notes:
        meta.append(["Notes:", asset.notes, "", ""])

    col_widths = [1.1 * inch, 2.4 * inch, 1.2 * inch, 2.0 * inch]
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
        ("SPAN", (1, -1), (3, -1)),  # notes spans last 3 cols if present
    ]))
    elements.append(meta_tbl)
    elements.append(Spacer(1, 0.15 * inch))

    # Photos
    if images:
        elements.append(Paragraph("Photos:", ss["FieldLabel"]))
        elements.append(Spacer(1, 0.05 * inch))

        # 2-column grid of images
        img_size = 2.5 * inch
        row_cells = []
        photo_rows = []

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
            while len(row_cells) < 2:
                row_cells.append("")
            photo_rows.append(row_cells)

        if photo_rows:
            photo_tbl = Table(
                photo_rows,
                colWidths=[img_size + 0.2 * inch, img_size + 0.2 * inch],
            )
            photo_tbl.setStyle(TableStyle([
                ("ALIGN", (0, 0), (-1, -1), "CENTER"),
                ("VALIGN", (0, 0), (-1, -1), "MIDDLE"),
                ("GRID", (0, 0), (-1, -1), 0.3, colors.lightgrey),
                ("PADDING", (0, 0), (-1, -1), 4),
            ]))
            elements.append(photo_tbl)
    else:
        elements.append(Paragraph("No photos on file.", ss["FieldLabel"]))

    elements.append(Spacer(1, 0.2 * inch))
    elements.append(PageBreak())
    return elements


def generate_report(
    assets_with_files: list[tuple[Asset, list[AssetFile]]],
    output_path: str | pathlib.Path,
) -> None:
    output_path = str(output_path)
    ss = _styles()

    doc = _NumberedDoc(
        output_path,
        pagesize=LETTER,
        leftMargin=_MARGIN,
        rightMargin=_MARGIN,
        topMargin=_MARGIN,
        bottomMargin=_MARGIN,
    )

    total_value: float | None = None
    for asset, _ in assets_with_files:
        if asset.value_estimate is not None:
            total_value = (total_value or 0) + asset.value_estimate

    story = []
    story += _title_page(ss, len(assets_with_files), total_value)
    story += _toc_page(ss, assets_with_files)
    for asset, files in assets_with_files:
        story += _asset_page(ss, asset, files)

    doc.build(story)
