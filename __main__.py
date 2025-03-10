#! /usr/bin/env python
import argparse
import sys
import numpy as np
from PyQt5.QtWidgets import (QApplication, QMainWindow, QWidget, QVBoxLayout, QHBoxLayout,
                             QPushButton, QScrollBar, QLineEdit, QLabel, QRadioButton,
                             QSizePolicy)
from PyQt5.QtGui import QImage, QPixmap, QPainter, QColor, QMouseEvent
from PyQt5.QtCore import Qt, QRect, QPoint, QPointF, QSize, QRectF

class VolumeViewer(QMainWindow):
    def __init__(self, volume_data, nx, ny, nz):
        super().__init__()
        self.volume = volume_data
        self.nx, self.ny, self.nz = nx, ny, nz
        self.current_slice = 0
        self.orientation = 0  # 0=XY, 1=XZ, 2=YZ
        self.window_level = [np.min(self.volume), np.max(self.volume)]
        self.view_rect = None  # (x_min, x_max, y_min, y_max) in image coordinates
        self.dragging = False
        self.drag_start_pos = None
        self.last_pixmap_info = None  # (pixmap_rect, image_rect)
        self.initUI()

    def initUI(self):
        # Central widget and main layout
        central_widget = QWidget()
        self.setCentralWidget(central_widget)
        main_layout = QVBoxLayout(central_widget)

        # Control toolbar
        control_layout = QHBoxLayout()
        
        # Buttons and controls
        self.zoom_btn = QPushButton("Zoom", checkable=True)
        self.drag_btn = QPushButton("Drag", checkable=True)
        self.reset_btn = QPushButton("Reset")
        self.min_input = QLineEdit(str(self.window_level[0]))
        self.max_input = QLineEdit(str(self.window_level[1]))
        self.xy_radio = QRadioButton("XY")
        self.xz_radio = QRadioButton("XZ")
        self.yz_radio = QRadioButton("YZ")

        # Layout organization
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

        # Image display area
        display_layout = QHBoxLayout()
        self.scrollbar = QScrollBar(Qt.Vertical)
        self.image_label = QLabel()
        self.image_label.setSizePolicy(QSizePolicy.Expanding, QSizePolicy.Expanding)
        self.image_label.setAlignment(Qt.AlignCenter)
        self.image_label.setMinimumSize(400, 400)

        display_layout.addWidget(self.scrollbar)
        display_layout.addWidget(self.image_label)

        # Connect signals
        self.reset_btn.clicked.connect(self.reset_view)
        self.zoom_btn.clicked.connect(lambda: self.drag_btn.setChecked(False))
        self.drag_btn.clicked.connect(lambda: self.zoom_btn.setChecked(False))
        self.min_input.editingFinished.connect(self.update_window_level)
        self.max_input.editingFinished.connect(self.update_window_level)
        self.xy_radio.toggled.connect(lambda: self.set_orientation(0))
        self.xz_radio.toggled.connect(lambda: self.set_orientation(1))
        self.yz_radio.toggled.connect(lambda: self.set_orientation(2))
        self.scrollbar.valueChanged.connect(self.set_slice)

        # Final layout
        main_layout.addLayout(control_layout)
        main_layout.addLayout(display_layout)
        
        self.resize(800, 600)
        self.setWindowTitle('Volume Viewer')
        self.show()
        self.reset_view()

    def reset_view(self):
        self.window_level = [np.min(self.volume), np.max(self.volume)]
        self.min_input.setText(f"{self.window_level[0]:.2f}")
        self.max_input.setText(f"{self.window_level[1]:.2f}")
        slice_data = self.get_current_slice()
        h, w = slice_data.shape
        self.view_rect = (0.0, float(w), 0.0, float(h))
        self.update_display()

    def get_current_slice(self):
        if self.orientation == 0:  # XY
            return self.volume[self.current_slice]
        elif self.orientation == 1:  # XZ
            return self.volume[:, self.current_slice]
        return self.volume[:, :, self.current_slice]  # YZ

    def update_display(self):
        # Get image data
        slice_data = self.get_current_slice()
        h, w = slice_data.shape
        
        # Initialize view rectangle
        if self.view_rect is None:
            self.view_rect = (0, w, 0, h)
            
        x_min, x_max, y_min, y_max = self.view_rect
        x_min = int(x_min)
        x_max = int(x_max)
        y_min = int(y_min)
        y_max = int(y_max)
        view_w = x_max - x_min
        view_h = y_max - y_min
        
        # Apply window level to the visible region
        visible_data = slice_data[int(y_min):int(y_max), int(x_min):int(x_max)]
        data = np.clip((visible_data - self.window_level[0]) / 
                      (self.window_level[1] - self.window_level[0]), 0, 1)
        data = (data * 255).astype(np.uint8)

        # Create pixmap
        qimage = QImage(data.data, view_w, view_h, view_w, QImage.Format_Grayscale8)
        pixmap = QPixmap.fromImage(qimage)
        
        # Scale to exactly fill the canvas
        canvas_size = self.image_label.size()
        scaled_pix = pixmap.scaled(canvas_size, Qt.IgnoreAspectRatio, Qt.FastTransformation)
        
        self.image_label.setPixmap(scaled_pix)
        
        # Store mapping information
        self.last_pixmap_info = (
            QRectF(self.image_label.x(), self.image_label.y(), canvas_size.width(), canvas_size.height()),
            QRectF(x_min, y_min, view_w, view_h)
        )

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
        self.view_rect = None
        self.update_display()

    def update_window_level(self):
        try:
            self.window_level = [
                float(self.min_input.text()),
                float(self.max_input.text())
            ]
            self.update_display()
        except ValueError:
            pass

    def mousePressEvent(self, event: QMouseEvent):
        if self.image_label.underMouse():
            # Store initial view rectangle and precise start position
            self.drag_start_view = self.view_rect
            self.drag_start_pos = self.mapToImage(event.pos())
            self.dragging = True

    def mouseMoveEvent(self, event: QMouseEvent):
        if self.dragging and self.drag_btn.isChecked():
            # Get current position in image coordinates
            current_pos = self.mapToImage(event.pos())
            
            # Calculate delta in image space using floating-point precision
            dx = self.drag_start_pos.x() - current_pos.x()
            dy = self.drag_start_pos.y() - current_pos.y()
            
            current_width = self.get_current_width()
            current_height = self.get_current_height()

            x_min, x_max, y_min, y_max = self.drag_start_view
            
            potential_new_x_min = x_min + dx
            potential_new_x_max = x_max + dx
            potential_new_y_min = y_min + dy
            potential_new_y_max = y_max + dy

            if potential_new_x_min >= 0 and potential_new_x_max <= current_width:
                new_x_min = potential_new_x_min
                new_x_max = potential_new_x_max
            else:
                new_x_min = x_min
                new_x_max = x_max
            if potential_new_y_min >= 0 and potential_new_y_max <= current_height:
                new_y_min = potential_new_y_min
                new_y_max = potential_new_y_max
            else:
                new_y_min = y_min
                new_y_max = y_max
            
            self.view_rect = (new_x_min, new_x_max, new_y_min, new_y_max)
            #self.drag_start_pos = current_pos
            self.update_display()
        
        elif self.zoom_btn.isChecked():
            # Get current position in image coordinates
            current_pos = self.mapToImage(event.pos())
            
            # Draw zoom rectangle directly on image coordinates
            self.update_display()
            painter = QPainter(self.image_label.pixmap())
            painter.setPen(QColor(255, 0, 0))
            
            # Convert coordinates to widget space
            start = self.mapFromImage(self.drag_start_pos)
            end = self.mapFromImage(current_pos)
            rect = QRectF(start, end).normalized()
            
            painter.drawRect(rect)
            painter.end()
            self.image_label.repaint()
        
    def mouseReleaseEvent(self, event: QMouseEvent):
        if self.dragging and self.zoom_btn.isChecked():
            end_pos = self.mapToImage(event.pos())
            
            x_min = min(self.drag_start_pos.x(), end_pos.x())
            x_max = max(self.drag_start_pos.x(), end_pos.x())
            y_min = min(self.drag_start_pos.y(), end_pos.y())
            y_max = max(self.drag_start_pos.y(), end_pos.y())
            
            if x_max - x_min > 2 and y_max - y_min > 2:
                self.view_rect = (x_min, x_max, y_min, y_max)
                self.update_display()

        self.dragging = False

    def mapToImage(self, pos: QPoint):
        """Convert widget coordinates to image coordinates (floating-point)"""
        if not self.last_pixmap_info:
            return QPointF(0, 0)
            
        widget_rect, image_rect = self.last_pixmap_info
        
        # Calculate scaling factors
        scale_x = image_rect.width() / widget_rect.width()
        scale_y = image_rect.height() / widget_rect.height()
        
        # Convert coordinates with floating-point precision
        img_x = (pos.x() - widget_rect.x()) * scale_x + self.view_rect[0]
        img_y = (pos.y() - widget_rect.y()) * scale_y + self.view_rect[2]
        
        return QPointF(img_x, img_y)

    def mapFromImage(self, pos: QPointF):
        """Convert image coordinates to widget coordinates (floating-point)"""
        if not self.last_pixmap_info:
            return QPointF(0, 0)
            
        widget_rect, image_rect = self.last_pixmap_info
        
        # Calculate scaling factors
        scale_x = widget_rect.width() / image_rect.width()
        scale_y = widget_rect.height() / image_rect.height()
        
        # Convert coordinates with floating-point precision
        widget_x = (pos.x() - image_rect.x()) * scale_x
        widget_y = (pos.y() - image_rect.y()) * scale_y
        
        return QPointF(widget_x, widget_y)

    def get_current_width(self):
        slice_data = self.get_current_slice()
        return float(slice_data.shape[1])

    def get_current_height(self):
        slice_data = self.get_current_slice()
        return float(slice_data.shape[0])

def main():
    parser = argparse.ArgumentParser(
        description="3D Volume Viewer",
        formatter_class=argparse.ArgumentDefaultsHelpFormatter
    )
    parser.add_argument(
        "input_file",
        type=str,
        help="Path to raw volume file (float32 binary)"
    )
    parser.add_argument(
        "nx", type=int,
        help="Size in X dimension (width)"
    )
    parser.add_argument(
        "ny", type=int,
        help="Size in Y dimension (height)"
    )
    parser.add_argument(
        "nz", type=int,
        help="Size in Z dimension (depth)"
    )
    
    args = parser.parse_args()
    
    # Validate arguments
    if args.nx <= 0 or args.ny <= 0 or args.nz <= 0:
        raise ValueError("All dimensions must be positive integers")
    
    try:
        volume = load_volume(args.input_file, args.nx, args.ny, args.nz)
    except FileNotFoundError:
        raise FileNotFoundError(f"Input file not found: {args.input_file}")
    
    app = QApplication(sys.argv)
    viewer = VolumeViewer(volume, args.nx, args.ny, args.nz)
    sys.exit(app.exec_())

def load_volume(filename, nx, ny, nz):
    """Load volume from raw binary file"""
    try:
        data = np.fromfile(filename, dtype=np.float32)
        expected_size = nx * ny * nz
        if len(data) != expected_size:
            raise ValueError(
                f"File size mismatch. Expected {expected_size} elements "
                f"({nx}x{ny}x{nz}), got {len(data)}"
            )
        return data.reshape((nz, ny, nx))  # Note ZYX ordering for numpy
    except Exception as e:
        raise RuntimeError(f"Error loading volume: {str(e)}")

if __name__ == '__main__':
    main()
