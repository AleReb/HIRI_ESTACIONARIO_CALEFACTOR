#!/usr/bin/env python3
"""Genera el PDF del manual desde su Markdown. Requiere reportlab y DejaVu Sans.
Uso: python3 documentacion/herramientas/generar_manual_pdf.py
"""
from pathlib import Path
from xml.sax.saxutils import escape
import re
import textwrap
from reportlab.lib import colors
from reportlab.lib.enums import TA_LEFT
from reportlab.lib.pagesizes import A4
from reportlab.lib.styles import getSampleStyleSheet, ParagraphStyle
from reportlab.pdfbase import pdfmetrics
from reportlab.pdfbase.ttfonts import TTFont
from reportlab.platypus import SimpleDocTemplate, Paragraph, Spacer, PageBreak, Table, TableStyle, Preformatted
from reportlab.platypus.tableofcontents import TableOfContents

DOCS=Path(__file__).resolve().parents[1]
SOURCE=DOCS/'manual_tecnico_hiri_calefactor_v2.md'
TARGET=SOURCE.with_suffix('.pdf')
FONT=Path('/usr/share/fonts/truetype/dejavu')
for name,file in [('DV','DejaVuSans.ttf'),('DVB','DejaVuSans-Bold.ttf'),('DVM','DejaVuSansMono.ttf')]:
    pdfmetrics.registerFont(TTFont(name,str(FONT/file)))
pdfmetrics.registerFontFamily('DV',normal='DV',bold='DVB',italic='DV',boldItalic='DVB')
NAVY=colors.HexColor('#153449'); TEAL=colors.HexColor('#087d89')
styles=getSampleStyleSheet()
styles.add(ParagraphStyle(name='BodyManual',fontName='DV',fontSize=9,leading=14,spaceAfter=7,textColor=NAVY))
styles.add(ParagraphStyle(name='SectionManual',fontName='DVB',fontSize=16,leading=21,spaceBefore=17,spaceAfter=11,keepWithNext=True,textColor=TEAL))
styles.add(ParagraphStyle(name='SubManual',fontName='DVB',fontSize=11,leading=16,spaceBefore=12,spaceAfter=7,keepWithNext=True,textColor=NAVY))
styles.add(ParagraphStyle(name='CellManual',fontName='DV',fontSize=7.6,leading=11,textColor=NAVY,wordWrap='CJK'))
styles.add(ParagraphStyle(name='CodeManual',fontName='DVM',fontSize=7,leading=10,spaceBefore=5,spaceAfter=10,backColor=colors.HexColor('#eef4f6'),borderPadding=7))
styles.add(ParagraphStyle(name='CoverTitle',fontName='DVB',fontSize=29,leading=37,textColor=NAVY,spaceAfter=20))
styles.add(ParagraphStyle(name='CoverSub',fontName='DV',fontSize=14,leading=22,textColor=TEAL,spaceAfter=15))
styles.add(ParagraphStyle(name='TocManual',fontName='DV',fontSize=10,leading=16,spaceBefore=7,textColor=NAVY))

def inline(text):
    text=escape(text)
    text=re.sub(r'`([^`]+)`',r'<font name="DVM">\1</font>',text)
    return text

class ManualDoc(SimpleDocTemplate):
    def afterFlowable(self,flowable):
        if isinstance(flowable,Paragraph) and flowable.style.name=='SectionManual' and re.match(r'^\d+\.', flowable.getPlainText()):
            title=flowable.getPlainText(); key='section-'+title.split('.')[0]
            self.canv.bookmarkPage(key)
            self.canv.addOutlineEntry(title,key,0,False)
            self.notify('TOCEntry',(0,title,self.page,key))

def page(canvas,doc):
    canvas.saveState(); w,h=A4
    if doc.page>1:
        canvas.setStrokeColor(TEAL);canvas.line(44,h-36,w-44,h-36)
        canvas.setFont('DV',7);canvas.setFillColor(NAVY)
        canvas.drawString(44,h-28,'HIRI AUCA · Manual técnico de consulta')
        canvas.drawRightString(w-44,h-28,'HIRI CAL V1.0.2 / Calefactor 1.0.0')
    canvas.setFont('DV',7);canvas.setFillColor(NAVY)
    canvas.drawString(44,25,'Revisión 17-11-2026 · Documentación basada en código')
    canvas.drawRightString(w-44,25,str(doc.page))
    canvas.restoreState()

story=[Spacer(1,72),Paragraph('HIRI AUCA',styles['CoverTitle']),Paragraph('Manual técnico<br/>de consulta',styles['CoverTitle']),Paragraph('Estación ESP32 y módulo calefactor V2',styles['CoverSub']),Spacer(1,15)]
for line in ['Base: HIRI_STATIC_CALEFACTOR_0_1 · CAL V1.0.2','Calefactor_V2_0_0 · firmware 1.0.0','17 de noviembre de 2026 · Código fuente inicial: 5e726a1']:
    story.append(Paragraph(line,styles['BodyManual']))
story.extend([Spacer(1,25),Paragraph('Componentes · Sensores · Pantalla y botón · Tiempos<br/>Consola USB · Protocolo serial · Servidor · SD y fechas<br/>Operación con y sin conexión · Mapa del firmware',styles['CoverSub']),Spacer(1,20),Paragraph('Describe el comportamiento ejecutable revisado. Las funciones heredadas o sin llamada activa se identifican expresamente.',styles['BodyManual']),PageBreak(),Paragraph('Índice de consulta',styles['SectionManual'])])
toc=TableOfContents();toc.levelStyles=[styles['TocManual']];story.extend([toc,PageBreak()])
lines=SOURCE.read_text().splitlines();i=0;width=A4[0]-88
while i<len(lines):
    line=lines[i]
    if not line.strip():i+=1;continue
    if line.startswith('# '):
        i+=1
        while i<len(lines) and not lines[i].startswith('## '):i+=1
        continue
    if line.startswith('```'):
        i+=1;block=[]
        while i<len(lines) and not lines[i].startswith('```'):
            block.extend(textwrap.wrap(lines[i],width=106,replace_whitespace=False,drop_whitespace=False) or ['']);i+=1
        story.append(Preformatted('\n'.join(block),styles['CodeManual']));i+=1;continue
    if line.startswith('## '):
        if line.startswith('## 14. '):
            story.append(PageBreak())
        story.append(Paragraph(inline(line[3:]),styles['SectionManual']));i+=1;continue
    if line.startswith('### '):
        story.append(Paragraph(inline(line[4:]),styles['SubManual']));i+=1;continue
    if line.startswith('|'):
        rows=[]
        while i<len(lines) and lines[i].startswith('|'):
            cells=[x.strip() for x in lines[i].strip().strip('|').split('|')]
            if not all(re.fullmatch(r':?-+:?',c) for c in cells):rows.append(cells)
            i+=1
        n=len(rows[0]);weights=[]
        for j in range(n):
            # Equilibra columnas largas sin dejar las de unidades demasiado estrechas.
            lengths=sorted(len(row[j]) for row in rows)
            typical=lengths[int((len(lengths)-1)*0.8)]
            weights.append(max(8,min(52,typical))**0.65)
        widths=[width*x/sum(weights) for x in weights]
        data=[[Paragraph(inline(c),styles['CellManual']) for c in row] for row in rows]
        table=Table(data,colWidths=widths,repeatRows=1,hAlign='LEFT')
        table.setStyle(TableStyle([('BACKGROUND',(0,0),(-1,0),colors.HexColor('#d9eaee')),('ROWBACKGROUNDS',(0,1),(-1,-1),[colors.white,colors.HexColor('#f4f7f8')]),('VALIGN',(0,0),(-1,-1),'TOP'),('LINEBELOW',(0,0),(-1,0),0.7,TEAL),('LINEBELOW',(0,1),(-1,-1),0.25,colors.HexColor('#d9e3e7')),('LEFTPADDING',(0,0),(-1,-1),7),('RIGHTPADDING',(0,0),(-1,-1),7),('TOPPADDING',(0,0),(-1,-1),6),('BOTTOMPADDING',(0,0),(-1,-1),6)]))
        story.extend([table,Spacer(1,10)]);continue
    paragraph=[line];i+=1
    if not re.match(r'^\d+\. ',line):
        while i<len(lines) and lines[i].strip() and not lines[i].startswith(('#','|','```')):
            paragraph.append(lines[i]);i+=1
    story.append(Paragraph(inline(' '.join(paragraph)),styles['BodyManual']))

doc=ManualDoc(str(TARGET),pagesize=A4,rightMargin=44,leftMargin=44,topMargin=51,bottomMargin=44,title='Manual técnico HIRI AUCA y calefactor V2',author='Proyecto HIRI AUCA',subject='Consulta técnica basada en firmware HIRI CAL V1.0.2 y calefactor 1.0.0')
doc.multiBuild(story,onFirstPage=page,onLaterPages=page)
print(TARGET)
