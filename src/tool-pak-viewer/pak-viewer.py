#!/usr/bin/env python3
"""
PAK Archive Viewer - A GUI tool for inspecting PAK archives
Uses PySide6 for the UI and calls pak-info/pak-unmake CLI tools
"""

import os
import re
import subprocess
import sys
import tempfile
from pathlib import Path
from typing import List

from PySide6.QtCore import Qt, Signal, QThread
from PySide6.QtGui import QPixmap
from PySide6.QtWidgets import (
    QApplication, QMainWindow, QTableWidgetItem,
    QFileDialog, QMessageBox, QProgressBar
)

# Import the generated UI
from pak_viewer_ui import Ui_MainWindow


class FileEntry:
    """Represents a file entry in the PAK archive"""
    def __init__(self, name: str, compressed: int, uncompressed: int, ratio: float, offset: int):
        self.name = name
        self.compressed_size = compressed
        self.uncompressed_size = uncompressed
        self.compression_ratio = ratio
        self.offset = offset


class ExtractionThread(QThread):
    """Background thread for extracting files"""
    finished = Signal(bool, str)  # success, file_path or error

    def __init__(self, pak_unmake_path: str, pak_file: str, entry_name: str, temp_dir: str):
        super().__init__()
        self.pak_unmake_path = pak_unmake_path
        self.pak_file = pak_file
        self.entry_name = entry_name
        self.temp_dir = temp_dir

    def run(self):
        try:
            # Extract to temp directory
            result = subprocess.run(
                [self.pak_unmake_path, self.pak_file, self.temp_dir],
                capture_output=True,
                text=True,
                timeout=30
            )

            if result.returncode != 0:
                self.finished.emit(False, f"Extraction failed: {result.stderr}")
                return

            # Find the extracted file
            extracted_path = Path(self.temp_dir) / self.entry_name
            if extracted_path.exists():
                self.finished.emit(True, str(extracted_path))
            else:
                self.finished.emit(False, f"File not found after extraction: {extracted_path}")

        except subprocess.TimeoutExpired:
            self.finished.emit(False, "Extraction timed out")
        except Exception as e:
            self.finished.emit(False, f"Error: {str(e)}")


class PakViewer(QMainWindow):
    """Main window for the PAK viewer application"""

    def __init__(self):
        super().__init__()
        self.pak_file = None
        self.entries: List[FileEntry] = []
        self.temp_dir = None
        self.current_extraction_thread = None

        # Setup UI from generated code
        self.ui = Ui_MainWindow()
        self.ui.setupUi(self)

        # Find tool executables
        self.find_tools()

        # Setup connections
        self.setup_connections()

        # Setup additional UI elements
        self.setup_ui()

    def find_tools(self):
        """Locate pak-info and pak-unmake executables"""

        script_dir = Path(__file__).parent.resolve()
        self.pak_info_path = script_dir / "pak-info"
        self.pak_make_path = script_dir / "pak-make"
        self.pak_unmake_path = script_dir / "pak-unmake"

    def setup_connections(self):
        """Connect UI signals to slots"""
        self.ui.actionOpen.triggered.connect(self.open_pak)
        self.ui.actionQuit.triggered.connect(self.close)
        self.ui.fileTable.itemSelectionChanged.connect(self.on_selection_changed)

    def setup_ui(self):
        """Setup additional UI elements not in the .ui file"""
        # Progress bar in status bar
        self.progress_bar = QProgressBar()
        self.progress_bar.setVisible(False)
        self.ui.statusbar.addPermanentWidget(self.progress_bar)

        # Show warning if tools are not available
        if not (self.pak_info_path and self.pak_unmake_path):
            missing = []
            if not self.pak_info_path:
                missing.append("pak-info")
            if not self.pak_unmake_path:
                missing.append("pak-unmake")
            QMessageBox.warning(
                self,
                "Tools Not Found",
                f"Could not find: {', '.join(missing)}\n\n"
                "Please build the project or ensure the tools are in your PATH."
            )

    def open_pak(self):
        """Open a PAK file"""
        file_path, _ = QFileDialog.getOpenFileName(
            self,
            "Open PAK Archive",
            "",
            "PAK Files (*.pak);;All Files (*)"
        )

        if file_path:
            self.load_pak(file_path)

    def load_pak(self, file_path: str):
        """Load and parse a PAK file"""
        if not self.pak_info_path:
            QMessageBox.critical(self, "Error", "pak-info tool not found")
            return

        self.ui.statusbar.showMessage(f"Loading {file_path}...")
        self.pak_file = file_path
        self.entries.clear()
        self.ui.fileTable.setRowCount(0)
        self.ui.previewText.clear()
        self.ui.previewImage.hide()

        # Clean up previous temp directory
        if self.temp_dir and os.path.exists(self.temp_dir):
            import shutil
            shutil.rmtree(self.temp_dir, ignore_errors=True)

        try:
            # Run pak-info
            result = subprocess.run(
                [self.pak_info_path, file_path],
                capture_output=True,
                text=True,
                timeout=10
            )

            if result.returncode != 0:
                QMessageBox.critical(self, "Error", f"Failed to read PAK file:\n{result.stderr}")
                return

            # Parse output
            self.parse_pak_info(result.stdout)

            # Update info label
            file_count = len(self.entries)
            total_compressed = sum(e.compressed_size for e in self.entries)
            total_uncompressed = sum(e.uncompressed_size for e in self.entries)

            info_text = f"<b>File:</b> {os.path.basename(file_path)} | "
            info_text += f"<b>Files:</b> {file_count} | "
            info_text += f"<b>Compressed:</b> {self.format_size(total_compressed)} | "
            info_text += f"<b>Uncompressed:</b> {self.format_size(total_uncompressed)}"

            self.ui.infoLabel.setText(info_text)
            self.ui.statusbar.showMessage(f"Loaded {file_count} file(s)")

        except subprocess.TimeoutExpired:
            QMessageBox.critical(self, "Error", "pak-info timed out")
        except Exception as e:
            QMessageBox.critical(self, "Error", f"Error loading PAK: {str(e)}")

    def parse_pak_info(self, output: str):
        """Parse pak-info output and populate the table"""
        lines = output.split('\n')

        # Find the table section
        in_table = False
        for line in lines:
            if '----' in line and in_table:
                continue  # Skip separator line
            elif 'Filename' in line and 'Compressed' in line:
                in_table = True
                continue
            elif in_table and line.strip() and not line.startswith('Total:'):
                # Parse file entry
                match = re.match(r'(\S.*?)\s+(\d+)\s+(\d+)\s+([-\d.]+)%\s+(\d+)', line)
                if match:
                    name = match.group(1).strip()
                    compressed = int(match.group(2))
                    uncompressed = int(match.group(3))
                    ratio = float(match.group(4))
                    offset = int(match.group(5))

                    entry = FileEntry(name, compressed, uncompressed, ratio, offset)
                    self.entries.append(entry)

                    # Add to table
                    row = self.ui.fileTable.rowCount()
                    self.ui.fileTable.insertRow(row)

                    self.ui.fileTable.setItem(row, 0, QTableWidgetItem(name))
                    self.ui.fileTable.setItem(row, 1, QTableWidgetItem(self.format_size(compressed)))
                    self.ui.fileTable.setItem(row, 2, QTableWidgetItem(self.format_size(uncompressed)))
                    self.ui.fileTable.setItem(row, 3, QTableWidgetItem(f"{ratio:.1f}%"))
                    self.ui.fileTable.setItem(row, 4, QTableWidgetItem(str(offset)))
            elif in_table and line.startswith('Total:'):
                break

    def on_selection_changed(self):
        """Handle file selection in the table"""
        selected = self.ui.fileTable.selectedItems()
        if not selected:
            return

        row = selected[0].row()
        if row < 0 or row >= len(self.entries):
            return

        entry = self.entries[row]
        self.preview_file(entry)

    def preview_file(self, entry: FileEntry):
        """Preview the selected file"""
        if not self.pak_unmake_path:
            self.ui.previewText.setPlainText("pak-unmake tool not found - cannot preview files")
            self.ui.previewText.show()
            self.ui.previewImage.hide()
            return

        self.ui.statusbar.showMessage(f"Extracting {entry.name}...")
        self.progress_bar.show()
        self.progress_bar.setRange(0, 0)  # Indeterminate

        # Create temp directory if needed
        if not self.temp_dir:
            self.temp_dir = tempfile.mkdtemp(prefix="pak_viewer_")

        # Start extraction in background
        self.current_extraction_thread = ExtractionThread(
            self.pak_unmake_path,
            self.pak_file,
            entry.name,
            self.temp_dir
        )
        self.current_extraction_thread.finished.connect(
            lambda success, path: self.on_extraction_finished(success, path, entry))
        self.current_extraction_thread.start()

    def on_extraction_finished(self, success: bool, file_path: str, entry: FileEntry):
        """Handle extraction completion"""
        self.progress_bar.hide()

        if not success:
            self.ui.previewText.setPlainText(f"Error: {file_path}")
            self.ui.previewText.show()
            self.ui.previewImage.hide()
            self.ui.statusbar.showMessage("Extraction failed")
            return

        # Try to preview the file
        ext = os.path.splitext(entry.name)[1].lower()

        # Image files
        if ext in ['.png', '.jpg', '.jpeg', '.bmp', '.gif']:
            pixmap = QPixmap(file_path)
            if not pixmap.isNull():
                # Scale to fit preview area while maintaining aspect ratio
                scaled = pixmap.scaled(
                    self.ui.previewImage.size(),
                    Qt.KeepAspectRatio,
                    Qt.SmoothTransformation
                )
                self.ui.previewImage.setPixmap(scaled)
                self.ui.previewText.hide()
                self.ui.previewImage.show()
                self.ui.statusbar.showMessage(f"Image: {pixmap.width()}x{pixmap.height()}")
            else:
                self.ui.previewText.setPlainText(f"Failed to load image: {entry.name}")
                self.ui.previewText.show()
                self.ui.previewImage.hide()

        # Text files
        elif ext in ['.txt', '.json', '.xml', '.toml', '.yaml', '.yml', '.md', '.cfg', '.ini', '.log']:
            try:
                with open(file_path, 'r', encoding='utf-8') as f:
                    content = f.read(100000)  # Limit to 100KB
                    if len(content) == 100000:
                        content += "\n\n... (truncated)"
                    self.ui.previewText.setPlainText(content)
                    self.ui.previewText.show()
                    self.ui.previewImage.hide()
                    self.ui.statusbar.showMessage(f"Text file: {len(content)} characters")
            except Exception as e:
                self.ui.previewText.setPlainText(f"Error reading file: {str(e)}")
                self.ui.previewText.show()
                self.ui.previewImage.hide()

        # Binary or unknown
        else:
            try:
                file_size = os.path.getsize(file_path)
                with open(file_path, 'rb') as f:
                    data = f.read(1024)

                # Show hex dump
                hex_lines = []
                for i in range(0, len(data), 16):
                    hex_part = ' '.join(f'{b:02x}' for b in data[i:i+16])
                    ascii_part = ''.join(chr(b) if 32 <= b < 127 else '.' for b in data[i:i+16])
                    hex_lines.append(f'{i:08x}  {hex_part:<48}  {ascii_part}')

                preview_text = f"Binary file ({self.format_size(file_size)})\n\n"
                preview_text += "Hex dump (first 1KB):\n"
                preview_text += '\n'.join(hex_lines)

                if file_size > 1024:
                    preview_text += "\n\n... (truncated)"

                self.ui.previewText.setPlainText(preview_text)
                self.ui.previewText.show()
                self.ui.previewImage.hide()
                self.ui.statusbar.showMessage(f"Binary file: {self.format_size(file_size)}")
            except Exception as e:
                self.ui.previewText.setPlainText(f"Error reading file: {str(e)}")
                self.ui.previewText.show()
                self.ui.previewImage.hide()

    def format_size(self, size: int) -> str:
        """Format file size in human-readable format"""
        if size < 1024:
            return f"{size} B"
        elif size < 1024 * 1024:
            return f"{size / 1024:.1f} KiB"
        else:
            return f"{size / (1024 * 1024):.1f} MiB"

    def closeEvent(self, event):
        """Clean up when closing"""
        # Cancel any ongoing extraction
        if self.current_extraction_thread and self.current_extraction_thread.isRunning():
            self.current_extraction_thread.terminate()
            self.current_extraction_thread.wait()

        # Clean up temp directory
        if self.temp_dir and os.path.exists(self.temp_dir):
            import shutil
            shutil.rmtree(self.temp_dir, ignore_errors=True)

        event.accept()


def main():
    app = QApplication(sys.argv)
    app.setApplicationName("PAK Viewer")

    viewer = PakViewer()
    viewer.show()

    # Open file from command line if provided
    if len(sys.argv) > 1:
        viewer.load_pak(sys.argv[1])

    sys.exit(app.exec())


if __name__ == '__main__':
    main()
