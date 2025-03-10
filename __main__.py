#!/usr/bin/env python
import sys
import numpy as np
from PyQt5.QtWidgets import (QApplication, QMainWindow, QWidget, QVBoxLayout, QHBoxLayout,
                             QPushButton, QScrollBar, QLineEdit, QLabel, QRadioButton,
                             QSizePolicy)
from PyQt5.QtGui import QImage, QPixmap, QPainter, QColor
from PyQt5.QtCore import Qt, QRect, QPoint

class VolumeViewer(QMainWindow):
    def __init__(self, volume_data, nx, ny, nz):
        super().__init__()
        self.volume = volume_data
        self.nx, self.ny, self.nz = nx, ny, nz
        self.current_slice = 0
        self.orientation = 0  # 0=XY, 1=XZ, 2=YZ
        self.window_level = [np.min(self.volume), np.max(self.volume)]
        self.zoom_factor = 1.0
        self.pan_offset = [0, 0]
        self.dragging = False
        self.drag_start = None
        self.initUI()

    def initUI(self):
        # Central widget and main layout
        central_widget = QWidget()
        self.setCentralWidget(central_widget)
        main_layout = QVBoxLayout(central_widget)

        # Control toolbar
        control_layout = QHBoxLayout()
        
        # Zoom/Drag buttons
        self.zoom_btn = QPushButton("Zoom", checkable=True)
        self.drag_btn = QPushButton("Drag", checkable=True)
        self.reset_btn = QPushButton("Reset")
        self.zoom_btn.clicked.connect(lambda: self.drag_btn.setChecked(False))
        self.drag_btn.clicked.connect(lambda: self.zoom_btn.setChecked(False))
        self.reset_btn.clicked.connect(self.reset_view)
        
        # Window level controls
        self.min_input = QLineEdit(str(self.window_level[0]))
        self.max_input = QLineEdit(str(self.window_level[1]))
        self.min_input.editingFinished.connect(self.update_window_level)
        self.max_input.editingFinished.connect(self.update_window_level)
        
        # Orientation radio buttons
        self.xy_radio = QRadioButton("XY")
        self.xz_radio = QRadioButton("XZ")
        self.yz_radio = QRadioButton("YZ")
        self.xy_radio.setChecked(True)
        self.xy_radio.toggled.connect(lambda: self.set_orientation(0))
        self.xz_radio.toggled.connect(lambda: self.set_orientation(1))
        self.yz_radio.toggled.connect(lambda: self.set_orientation(2))
        
        # Assemble controls
        control_layout.addWidget(self.zoom_btn)
        control_layout.addWidget(self.drag_btn)
        control_layout.addWidget(self.reset_btn)
        control_layout.addWidget(QLabel("Min:"))
        control_layout.addWidget(self.min_input)
        control_layout.addWidget(QLabel("Max:"))
        control_layout.addWidget(self.max_input)
        control_layout.addWidget(self.xy_radio)
        control_layout.addWidget(self.xz_radio)
        control_layout.addWidget(self.yz_radio)

        # Image display area with scrollbar
        image_layout = QHBoxLayout()
        
        # Vertical scrollbar on the left
        self.scrollbar = QScrollBar(Qt.Vertical)
        self.scrollbar.setMaximum(self.nz-1)
        self.scrollbar.valueChanged.connect(self.set_slice)
        
        # Image display
        self.image_label = QLabel()
        self.image_label.setSizePolicy(QSizePolicy.Expanding, QSizePolicy.Expanding)
        self.image_label.setAlignment(Qt.AlignCenter)
        self.image_label.setMinimumSize(400, 400)

        image_layout.addWidget(self.scrollbar)
        image_layout.addWidget(self.image_label)

        # Add widgets to main layout
        main_layout.addLayout(control_layout)
        main_layout.addLayout(image_layout)
        
        self.update_display()
        self.setWindowTitle('Volume Viewer')
        self.resize(800, 600)
        self.show()

    def reset_view(self):
        self.zoom_factor = 1.0
        self.pan_offset = [0, 0]
        self.window_level = [np.min(self.volume), np.max(self.volume)]
        self.min_input.setText(str(self.window_level[0]))
        self.max_input.setText(str(self.window_level[1]))
        self.update_display()

    def update_display(self):
        # Get current slice data
        if self.orientation == 0:  # XY
            slice_data = self.volume[self.current_slice, :, :]
        elif self.orientation == 1:  # XZ
            slice_data = self.volume[:, self.current_slice, :]
        else:  # YZ
            slice_data = self.volume[:, :, self.current_slice]

        # Apply window level
        data = np.clip((slice_data - self.window_level[0]) / 
                      (self.window_level[1] - self.window_level[0]), 0, 1)
        data = (data * 255).astype(np.uint8)

        # Create QImage
        height, width = data.shape
        qimage = QImage(data.data, width, height, width, QImage.Format_Grayscale8)
        pixmap = QPixmap.fromImage(qimage)
        
        # Apply zoom without interpolation
        scaled_pixmap = pixmap.scaled(
            int(width * self.zoom_factor),
            int(height * self.zoom_factor),
            Qt.IgnoreAspectRatio,
            Qt.FastTransformation
        )
        
        # Create final pixmap
        final_pixmap = QPixmap(self.image_label.size())
        final_pixmap.fill(Qt.white)
        painter = QPainter(final_pixmap)
        
        # Calculate draw coordinates with integer values
        draw_x = int(self.pan_offset[0])
        draw_y = int(self.pan_offset[1])
        
        # Clamp pan values
        max_x = final_pixmap.width() - scaled_pixmap.width()
        max_y = final_pixmap.height() - scaled_pixmap.height()
        draw_x = min(max(draw_x, -scaled_pixmap.width()), max_x)
        draw_y = min(max(draw_y, -scaled_pixmap.height()), max_y)
        
        painter.drawPixmap(draw_x, draw_y, scaled_pixmap)
        painter.end()
        
        self.image_label.setPixmap(final_pixmap)

    def set_slice(self, value):
        self.current_slice = value
        self.update_display()

    def set_orientation(self, orientation):
        self.orientation = orientation
        max_slice = {
            0: self.nz-1,
            1: self.ny-1,
            2: self.nx-1
        }[orientation]
        self.scrollbar.setMaximum(max_slice)
        self.current_slice = min(self.current_slice, max_slice)
        self.scrollbar.setValue(self.current_slice)
        self.update_display()

    def update_window_level(self):
        try:
            min_val = float(self.min_input.text())
            max_val = float(self.max_input.text())
            self.window_level = [min_val, max_val]
            self.update_display()
        except ValueError:
            pass

    def mousePressEvent(self, event):
        if self.image_label.underMouse():
            self.drag_start = event.pos()
            self.drag_start_pan = self.pan_offset.copy()
            self.dragging = True

    def mouseMoveEvent(self, event):
        if self.dragging and self.drag_start:
            if self.drag_btn.isChecked():
                # Panning
                delta = event.pos() - self.drag_start
                self.pan_offset = [
                    self.drag_start_pan[0] + delta.x(),
                    self.drag_start_pan[1] + delta.y()
                ]
                self.update_display()
            elif self.zoom_btn.isChecked():
                # Draw zoom rectangle
                self.update_display()
                painter = QPainter(self.image_label.pixmap())
                painter.setPen(QColor(255, 0, 0))
                rect = QRect(self.drag_start, event.pos()).normalized()
                painter.drawRect(rect)
                painter.end()
                self.image_label.repaint()

    def mouseReleaseEvent(self, event):
        if self.dragging and self.zoom_btn.isChecked():
            # Calculate zoom area in image coordinates
            start_x = (self.drag_start.x() - self.pan_offset[0]) / self.zoom_factor
            start_y = (self.drag_start.y() - self.pan_offset[1]) / self.zoom_factor
            end_x = (event.pos().x() - self.pan_offset[0]) / self.zoom_factor
            end_y = (event.pos().y() - self.pan_offset[1]) / self.zoom_factor

            rect_width = abs(end_x - start_x)
            rect_height = abs(end_y - start_y)
            
            if rect_width > 10 and rect_height > 10:
                # Calculate new zoom factor
                view_width = self.image_label.width()
                view_height = self.image_label.height()
                zoom_x = view_width / rect_width
                zoom_y = view_height / rect_height
                self.zoom_factor = min(zoom_x, zoom_y, 5.0)
                
                # Calculate new pan offset
                self.pan_offset = [
                    int(-start_x * self.zoom_factor),
                    int(-start_y * self.zoom_factor)
                ]
                
                self.update_display()

        self.dragging = False
        self.drag_start = None

def load_volume(filename, nx, ny, nz):
    return np.fromfile(filename, dtype=np.float32).reshape((nz, ny, nx))

if __name__ == '__main__':
    app = QApplication(sys.argv)
    
    # Example usage with dummy data
    dummy_volume = np.random.rand(100, 100, 100).astype(np.float32)
    viewer = VolumeViewer(dummy_volume, 100, 100, 100)
    
    sys.exit(app.exec_())