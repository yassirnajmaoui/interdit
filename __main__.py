#! /usr/bin/env python
import argparse
import sys
import numpy as np
from PyQt5.QtWidgets import (
    QApplication,
    QMainWindow,
    QWidget,
    QVBoxLayout,
    QHBoxLayout,
    QPushButton,
    QScrollBar,
    QLineEdit,
    QLabel,
    QRadioButton,
    QCheckBox,
    QSizePolicy,
    QDialog,
)
from PyQt5.QtGui import QImage, QPixmap, QPainter, QColor, QMouseEvent
from PyQt5.QtCore import Qt, QPoint, QRectF, pyqtSignal, QPointF

# TODO: Sync view should also mean to sync the orientation
# TODO: The display should always take the size of the frame
#  if the window is resized, the display should refresh without
#  needing to click reset and losing the view


class VolumeViewer(QMainWindow):
    intensity_changed = pyqtSignal(tuple)
    view_rect_changed = pyqtSignal(tuple)
    slice_changed = pyqtSignal(int)
    orientation_changed = pyqtSignal(int)

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

        self.min_input.editingFinished.connect(self._emit_intensity_changed)
        self.max_input.editingFinished.connect(self._emit_intensity_changed)
        self.scrollbar.valueChanged.connect(lambda v: self.slice_changed.emit(v))
        self.xy_radio.toggled.connect(lambda: self._emit_orientation_change(0))
        self.xz_radio.toggled.connect(lambda: self._emit_orientation_change(1))
        self.yz_radio.toggled.connect(lambda: self._emit_orientation_change(2))

    def _emit_intensity_changed(self):
        try:
            min_val = float(self.min_input.text())
            max_val = float(self.max_input.text())
            self.intensity_changed.emit((min_val, max_val))
        except ValueError:
            pass

    def _emit_orientation_change(self, orientation):
        self.orientation = orientation
        self.orientation_changed.emit(orientation)

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
        self.xy_radio.toggled.connect(lambda: self.set_orientation(0, False))
        self.xz_radio.toggled.connect(lambda: self.set_orientation(1, False))
        self.yz_radio.toggled.connect(lambda: self.set_orientation(2, False))
        self.scrollbar.valueChanged.connect(self.set_slice)

        # Final layout
        main_layout.addLayout(control_layout)
        main_layout.addLayout(display_layout)

        self.resize(800, 600)
        self.setWindowTitle("Volume Viewer")
        self.show()
        self.reset_view()

    def reset_view(self):
        slice_data = self.get_current_slice()
        h, w = slice_data.shape
        self.view_rect = (0, w, 0, h)
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
        visible_data = slice_data[int(y_min) : int(y_max), int(x_min) : int(x_max)]
        data = np.clip(
            (visible_data - self.window_level[0])
            / (self.window_level[1] - self.window_level[0]),
            0,
            1,
        )
        data = (data * 255).astype(np.uint8)

        # Create pixmap
        qimage = QImage(data.data, view_w, view_h, view_w, QImage.Format_Grayscale8)
        pixmap = QPixmap.fromImage(qimage)

        # Scale to exactly fill the canvas
        canvas_size = self.image_label.size()
        scaled_pix = pixmap.scaled(
            canvas_size, Qt.IgnoreAspectRatio, Qt.FastTransformation
        )

        self.image_label.setPixmap(scaled_pix)

        # Store mapping information
        self.last_pixmap_info = (
            QRectF(
                self.image_label.x(),
                self.image_label.y(),
                canvas_size.width(),
                canvas_size.height(),
            ),
            QRectF(x_min, y_min, view_w, view_h),
        )

    def set_slice(self, value, internal=False):
        if not internal:
            self.slice_changed.emit(value)
        else:
            self.scrollbar.blockSignals(True)
            self.scrollbar.setValue(value)
            self.scrollbar.blockSignals(False)

        self.current_slice = value
        self.update_display()

    def set_orientation(self, orientation, internal=False):
        """Updated set_orientation method"""
        self.orientation = orientation

        if not internal:
            self.orientation_changed.emit(orientation)
        else:
            self.xy_radio.blockSignals(True)
            self.xz_radio.blockSignals(True)
            self.yz_radio.blockSignals(True)
            if orientation == 0:
                self.xy_radio.setChecked(True)
            elif orientation == 1:
                self.xz_radio.setChecked(True)
            else:
                self.yz_radio.setChecked(True)
            self.xy_radio.blockSignals(False)
            self.xz_radio.blockSignals(False)
            self.yz_radio.blockSignals(False)

        max_slice = {0: self.nz - 1, 1: self.ny - 1, 2: self.nx - 1}[orientation]
        self.scrollbar.setMaximum(max_slice)
        self.current_slice = min(self.current_slice, max_slice)
        self.scrollbar.setValue(self.current_slice)
        self.view_rect = None
        self.update_display()

    def set_view_rect(self, view_rect, internal=False):
        if not internal:
            self.view_rect_changed.emit(view_rect)
        self.view_rect = view_rect
        self.update_display()

    def update_window_level(self):
        try:
            self.set_window_level(
                float(self.min_input.text()), float(self.max_input.text())
            )
        except ValueError:
            pass

    def set_window_level(self, min_val, max_val, internal=False):
        if not internal:
            self.intensity_changed.emit((min_val, max_val))
        self.window_level = [min_val, max_val]
        self.update_display()

    def mousePressEvent(self, event: QMouseEvent):
        if self.image_label.underMouse():
            # Store initial view rectangle and precise start position
            self.drag_start_view = self.view_rect
            self.drag_start_pos = self.mapToImage(event.pos())
            self.drag_start_pos_map = event.pos()
            self.dragging = True

    def mouseMoveEvent(self, event: QMouseEvent):
        if self.dragging and self.drag_btn.isChecked():
            # Get current position in image coordinates
            current_pos_map = event.pos()

            # Calculate scaling factors
            widget_rect, image_rect = self.last_pixmap_info
            scale_x = image_rect.width() / widget_rect.width()
            scale_y = image_rect.height() / widget_rect.height()

            # Calculate delta in image space using floating-point precision
            dx = int((self.drag_start_pos_map.x() - current_pos_map.x()) * scale_x)
            dy = int((self.drag_start_pos_map.y() - current_pos_map.y()) * scale_y)

            if abs(dx) > 1 or abs(dy) > 1:
                current_width = self.get_current_width()
                current_height = self.get_current_height()

                x_min, x_max, y_min, y_max = self.drag_start_view

                potential_new_x_min = x_min + dx
                potential_new_x_max = x_max + dx
                potential_new_y_min = y_min + dy
                potential_new_y_max = y_max + dy

                image_needs_update = False

                if potential_new_x_min >= 0 and potential_new_x_max <= current_width:
                    new_x_min = potential_new_x_min
                    new_x_max = potential_new_x_max
                    image_needs_update = True
                else:
                    new_x_min = x_min
                    new_x_max = x_max

                if potential_new_y_min >= 0 and potential_new_y_max <= current_height:
                    new_y_min = potential_new_y_min
                    new_y_max = potential_new_y_max
                    image_needs_update = True
                else:
                    new_y_min = y_min
                    new_y_max = y_max

                if image_needs_update:
                    view_rect = (new_x_min, new_x_max, new_y_min, new_y_max)
                    self.set_view_rect(view_rect)

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

            x_min = int(min(self.drag_start_pos.x(), end_pos.x()))
            x_max = int(max(self.drag_start_pos.x(), end_pos.x()))
            y_min = int(min(self.drag_start_pos.y(), end_pos.y()))
            y_max = int(max(self.drag_start_pos.y(), end_pos.y()))

            current_width = self.get_current_width()
            current_height = self.get_current_height()

            x_min = clamp(x_min, 0, current_width)
            x_max = clamp(x_max, 0, current_width)
            y_min = clamp(y_min, 0, current_height)
            y_max = clamp(y_max, 0, current_height)

            if x_max - x_min > 2 and y_max - y_min > 2:
                view_rect = (x_min, x_max, y_min, y_max)
                self.set_view_rect(view_rect)

        self.dragging = False

    def mapToImage(self, pos: QPoint):
        """Convert widget coordinates to image coordinates (floating-point)"""
        if not self.last_pixmap_info:
            return QPoint(0, 0)

        widget_rect, image_rect = self.last_pixmap_info

        # Calculate scaling factors
        scale_x = image_rect.width() / widget_rect.width()
        scale_y = image_rect.height() / widget_rect.height()

        # Convert coordinates with floating-point precision
        img_x = int((pos.x() - widget_rect.x()) * scale_x + self.view_rect[0] + 0.5)
        img_y = int((pos.y() - widget_rect.y()) * scale_y + self.view_rect[2] + 0.5)

        return QPoint(img_x, img_y)

    def mapFromImage(self, pos: QPoint):
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


class SyncControl(QDialog):
    sync_toggled = pyqtSignal(str, bool)  # (sync_type, checked)

    def __init__(self, parent=None):
        super().__init__(parent)
        layout = QVBoxLayout()

        self.intensity_sync = QCheckBox("Sync Intensity")
        self.view_sync = QCheckBox("Sync View")
        self.slice_sync = QCheckBox("Sync Slice")

        # Connect checkboxes to signal
        for cb, name in [
            (self.intensity_sync, "intensity"),
            (self.view_sync, "view"),
            (self.slice_sync, "slice"),
        ]:
            cb.stateChanged.connect(
                lambda state, n=name: self.sync_toggled.emit(n, state == Qt.Checked)
            )

        layout.addWidget(self.intensity_sync)
        layout.addWidget(self.view_sync)
        layout.addWidget(self.slice_sync)
        self.setLayout(layout)
        self.setWindowTitle("Synchronization Controls")


class VolumeViewerManager:
    def __init__(self, volumes):
        self.viewers = []
        self.app = QApplication.instance() or QApplication(sys.argv)

        # Create sync control window
        self.sync_control = SyncControl()
        self.sync_control.show()

        # Create viewers
        for vol_data, nx, ny, nz in volumes:
            viewer = VolumeViewer(vol_data, nx, ny, nz)
            viewer.show()
            self._connect_viewer_signals(viewer)
            self.viewers.append(viewer)

        # Connect sync toggles
        self.sync_control.sync_toggled.connect(self.handle_sync_toggled)

    def handle_sync_toggled(self, sync_type, checked):
        if not checked or not self.viewers:
            return

        # Get reference viewer (first viewer)
        ref_viewer = self.viewers[0]

        # Sync all viewers to reference viewer's state
        if sync_type == "intensity":
            wl = (ref_viewer.window_level[0], ref_viewer.window_level[1])
            for viewer in self.viewers[1:]:
                viewer.set_window_level(*wl, internal=True)
        elif sync_type == "view":
            vr = ref_viewer.view_rect
            for viewer in self.viewers[1:]:
                viewer.set_view_rect(vr, internal=True)
        elif sync_type == "slice":
            sl = ref_viewer.current_slice
            for viewer in self.viewers[1:]:
                viewer.set_orientation(ref_viewer.orientation, internal=True)
                viewer.set_slice(sl, internal=True)
            # Then sync view rectangle and slice ??? TODO check if this makes sense
            # self._propagate_view_rect(ref_viewer, ref_viewer.view_rect)
            # self._propagate_slice(ref_viewer, ref_viewer.current_slice)

    def _connect_viewer_signals(self, viewer):
        viewer.intensity_changed.connect(
            lambda wl: self._propagate_intensity(viewer, wl)
        )
        viewer.view_rect_changed.connect(
            lambda vr: self._propagate_view_rect(viewer, vr)
        )
        viewer.slice_changed.connect(lambda s: self._propagate_slice(viewer, s))
        viewer.orientation_changed.connect(
            lambda o: self._propagate_orientation(viewer, o)
        )

    def _propagate_intensity(self, source, window_level):
        if self.sync_control.intensity_sync.isChecked():
            for viewer in self.viewers:
                if viewer != source:
                    viewer.set_window_level(*window_level, internal=True)

    def _propagate_orientation(self, source, orientation):
        """Propagate orientation changes to other viewers when view sync is on"""
        if self.sync_control.slice_sync.isChecked():
            for viewer in self.viewers:
                if viewer != source:
                    viewer.set_orientation(orientation, internal=True)

    def _propagate_view_rect(self, source, view_rect):
        if self.sync_control.view_sync.isChecked():
            for viewer in self.viewers:
                if viewer != source:
                    # Convert view rect to target viewer's coordinate system
                    target_width = viewer.get_current_width()
                    target_height = viewer.get_current_height()

                    # Calculate relative view rect
                    rel_x_min = view_rect[0] / source.get_current_width()
                    rel_x_max = view_rect[1] / source.get_current_width()
                    rel_y_min = view_rect[2] / source.get_current_height()
                    rel_y_max = view_rect[3] / source.get_current_height()

                    # Apply to target
                    new_vr = (
                        rel_x_min * target_width,
                        rel_x_max * target_width,
                        rel_y_min * target_height,
                        rel_y_max * target_height,
                    )
                    viewer.set_view_rect(new_vr, internal=True)

    def _propagate_slice(self, source, slice_idx):
        if self.sync_control.slice_sync.isChecked():
            for viewer in self.viewers:
                viewer.set_slice(slice_idx, internal=True)

    def get_orientation_max_slice(self, viewer):
        """Helper to get maximum slice for current orientation"""
        return {
            0: viewer.nz - 1,  # XY orientation: slices along Z
            1: viewer.ny - 1,  # XZ orientation: slices along Y
            2: viewer.nx - 1,  # YZ orientation: slices along X
        }[viewer.orientation]


def clamp(val, min, max):
    if val < min:
        return min
    if val > max:
        return max
    return val


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


def main():
    parser = argparse.ArgumentParser(description="Multi-Volume Viewer")
    parser.add_argument(
        "volumes", nargs="+", help="Volume files and dimensions as: file nx ny nz"
    )
    args = parser.parse_args()

    if len(args.volumes) % 4 != 0:
        raise ValueError("Expected groups of 4 arguments: file nx ny nz")

    volumes = []
    for i in range(0, len(args.volumes), 4):
        try:
            path, nx, ny, nz = args.volumes[i : i + 4]
            vol = load_volume(path, int(nx), int(ny), int(nz))
            volumes.append((vol, int(nx), int(ny), int(nz)))
        except Exception as e:
            raise ValueError(f"Invalid volume specification at position {i}: {e}")

    manager = VolumeViewerManager(volumes)
    sys.exit(manager.app.exec_())


if __name__ == "__main__":
    main()
