import PyPDF2
import sys

sys.stdout.reconfigure(encoding='utf-8', errors='replace')

pdf_path = r'D:\VFS\VFS_SDK\docs\VFS Plugin SDK.pdf'
reader = PyPDF2.PdfReader(pdf_path)

for i in range(94, 100):
    text = reader.pages[i].extract_text()
    if text:
        print(f"\n{'='*80}")
        print(f"PAGE {i+1}")
        print(f"{'='*80}")
        print(text)
