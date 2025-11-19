#                Toolchefs ltd - Software Disclaimer
#
# Copyright 2014 Toolchefs Limited
#
# The software, information, code, data and other materials (Software)
# contained in, or related to, these files is the confidential and proprietary
# information of Toolchefs ltd.
# The software is protected by copyright. The Software must not be disclosed,
# distributed or provided to any third party without the prior written
# authorisation of Toolchefs ltd.

"""
Motion Path Plugin for Maya 2025
Converted for Maya 2025 compatibility with PySide6 and Python 3
"""

import os
import traceback

# Maya 2025 uses PySide6
try:
    from PySide6 import QtGui, QtCore, QtWidgets
    from PySide6.QtCore import Signal, Slot, Qt
except ImportError:
    # Fallback to PySide2 for Maya 2022-2024
    try:
        from PySide2 import QtGui, QtCore, QtWidgets
        from PySide2.QtCore import Signal, Slot, Qt
    except ImportError:
        # Last resort: PySide (Maya 2016-2017)
        from PySide import QtGui, QtCore
        import PySide.QtGui as QtWidgets
        from PySide.QtCore import Signal, Slot, Qt

from maya.app.general.mayaMixin import MayaQWidgetDockableMixin
from maya import OpenMayaUI
from maya import OpenMaya
from maya import cmds

# Workspace Control Name
WCN = "ToolchefsMotionPathWorkspaceControl"


class StaticLabels:
    """Configuration labels for Maya option variables"""
    FRAMES_BEFORE = "ToolChefs_MP_framesBefore"
    FRAMES_AFTER = "ToolChefs_MP_framesAfter"
    PATH_COLOR = "ToolChefs_MP_pathColor"
    WPATH_COLOR = "ToolChefs_MP_weightedPathColor"
    WTANGENT_COLOR = "ToolChefs_MP_weightedPathTangentColor"
    BPATH_COLOR = "ToolChefs_MP_bufferPathColor"
    CFRAME_COLOR = "ToolChefs_MP_cFrameColor"
    TANGENT_COLOR = "ToolChefs_MP_tangentColor"
    BTANGENT_COLOR = "ToolChefs_MP_bTangentColor"
    PATH_SIZE = "ToolChefs_MP_pathSize"
    FRAME_SIZE = "ToolChefs_MP_frameSize"
    SHOW_PATH = "ToolChefs_MP_showPath"
    SHOW_TANGENTS = "ToolChefs_MP_showTangents"
    SHOW_KEYFRAMES = "ToolChefs_MP_showKeyFrames"
    TIME_DELTA = "ToolChefs_MP_drawTimeDelta"
    FRAME_DELTA = "ToolChefs_MP_frameInterval"
    DRAW_KEYFRAME_COUNT = "ToolChefs_MP_drawKeyframeCount"
    SHOW_ROTATION = "ToolChefs_MP_showRotationKeyFrames"
    FNUMBER_COLOR = "ToolChefs_MP_frameNumberColor"
    KEYFRAME_NUMBER_COLOR = "ToolChefs_MP_keyframeNumberColor"
    SHOW_KNUMBER = "ToolChefs_MP_showKeyNumbers"
    SHOW_FNUMBER = "ToolChefs_MP_showFrameNumbers"
    ALTERNATE_COLOR = "ToolChefs_MP_alternatePathColor"
    USEPIVOTS = "ToolChefs_MP_usePivots"
    KEYFRAME_LABEL_SIZE = "ToolChefs_MP_keyframeLabelSize"
    FRAME_LABEL_SIZE = "ToolChefs_MP_frameLabelSize"


# Default configuration values
_default_values = {
    StaticLabels.FRAMES_BEFORE  : 20,
    StaticLabels.FRAMES_AFTER   : 20,
    StaticLabels.PATH_COLOR     : [0, 178, 238],
    StaticLabels.WPATH_COLOR    : [255, 0, 0],
    StaticLabels.WTANGENT_COLOR : [139, 0, 0],
    StaticLabels.CFRAME_COLOR   : [255, 255, 0],
    StaticLabels.TANGENT_COLOR  : [139, 105, 105],
    StaticLabels.BTANGENT_COLOR : [139, 69, 19],
    StaticLabels.BPATH_COLOR    : [51, 51, 51],
    StaticLabels.FNUMBER_COLOR  : [25, 25, 25],
    StaticLabels.KEYFRAME_NUMBER_COLOR : [255, 255, 0],
    StaticLabels.PATH_SIZE      : 3,
    StaticLabels.FRAME_SIZE     : 7,
    StaticLabels.SHOW_PATH      : True,
    StaticLabels.SHOW_TANGENTS  : False,
    StaticLabels.SHOW_KEYFRAMES : True,
    StaticLabels.SHOW_ROTATION  : True,
    StaticLabels.TIME_DELTA     : 1,
    StaticLabels.FRAME_DELTA    : 1,
    StaticLabels.DRAW_KEYFRAME_COUNT : 5,
    StaticLabels.SHOW_KNUMBER   : False,
    StaticLabels.SHOW_FNUMBER   : False,
    StaticLabels.ALTERNATE_COLOR: False,
    StaticLabels.USEPIVOTS      : False,
    StaticLabels.KEYFRAME_LABEL_SIZE : 1.2,
    StaticLabels.FRAME_LABEL_SIZE    : 0.8
}


class TOOLS:
    """Motion Path tool identifiers"""
    EDIT = 'motionPathEdit'
    DRAW = 'motionPathDraw'  # Kept for backward compatibility, not shown in UI

    @classmethod
    def all( cls ):
        """Return all available tools for UI buttons"""
        return [cls.EDIT]  # Only Edit button shown, Draw integrated via Caps Lock

    @classmethod
    def tooltip( cls, name ):
        """Return tooltip text for a given tool"""
        if name == cls.EDIT:
            return (
                '===== Edit Mode (Caps Lock OFF) =====\n'
                'Left-Click: Select/Move keyframe\n'
                'Shift+Left-Click: Add to selection\n'
                'Ctrl+Left-Click: Toggle selection\n'
                'Ctrl+MMB-Drag: Move along Y axis\n'
                'Right-Click: Context menu\n'
                '\n'
                '===== Draw Mode (Caps Lock ON) =====\n'
                'Left-Drag on keyframe: Create new keyframes\n'
                'Ctrl+Left-Drag: Adjust existing keyframes\n'
                '\n'
                '[Tip] Toggle Caps Lock to switch modes'
            )
        elif name == cls.DRAW:
            return ('Left-Click key frame then drag to draw path\n'
                    'CTRL-Left-Click key frame then drag to draw proximity stroke')
        return ""


# Global script job IDs
script_job_ids = []


#############################################################
# UTILITY FUNCTIONS
#############################################################

def _maya_api_version():
    """Get Maya API version as integer"""
    return int(cmds.about(api=True))


def _get_icons_path():
    """Get the path to the icons directory"""
    tokens = __file__.split(os.path.sep)
    return os.path.sep.join(tokens[:-1] + ['icons']) + os.path.sep


def _delete_script_jobs():
    """Delete all registered script jobs"""
    global script_job_ids
    for job_id in script_job_ids:
        if cmds.scriptJob(exists=job_id):
            cmds.scriptJob(kill=job_id)
    script_job_ids = []


def _playback_range():
    """Update motion path with current playback range"""
    if not _is_plugin_loaded():
        return

    min_time = cmds.playbackOptions(q=True, minTime=True)
    max_time = cmds.playbackOptions(q=True, maxTime=True)

    _safe_tcMotionPathCmd(frameRange=[min_time, max_time])
    _safe_tcMotionPathCmd(refreshdt=True)
    cmds.refresh()


def _delete_all_event():
    """Handle Maya's deleteAll event"""
    if tc_motion_path_widget is None:
        return
    tc_motion_path_widget.empty_buffer_paths()


def _script_job( callback, event=None, conditionChange=None, compressUndo=False ):
    """Register a Maya script job"""
    global script_job_ids

    job_id = None
    if conditionChange is not None:
        job_id = cmds.scriptJob(conditionChange=[conditionChange, callback])
    elif event is not None:
        job_id = cmds.scriptJob(event=[event, callback])

    if job_id:
        script_job_ids.append(job_id)


def _plugin_unload_callback(*args, **kwargs):
    """Callback when motionPath plugin is unloaded

    Cleans up viewport overrides and disables Motion Path UI
    """
    global tc_motion_path_widget

    print("[MotionPath] Plugin unloaded - cleaning up viewports")

    # Clean up viewport overrides
    _cleanup_viewport_overrides()

    # Disable UI if it exists
    if tc_motion_path_widget and hasattr(tc_motion_path_widget, 'mws'):
        try:
            if tc_motion_path_widget.mws._enable_button.isChecked():
                tc_motion_path_widget.mws._enable_button.setChecked(False)
                tc_motion_path_widget.mws._edit_buttons.setEnabled(False)
        except:
            pass

    # Force refresh
    try:
        cmds.refresh(force=True)
    except:
        pass


def _check_plugin_unload():
    """Check if motionPath plugin was unloaded"""
    if not _is_plugin_loaded():
        _plugin_unload_callback()


def _init_script_jobs():
    """Initialize all script jobs"""
    _script_job(_playback_range, event="playbackRangeChanged")
    _script_job(_tool_changed, event="ToolChanged")
    _script_job(_delete_all_event, event="deleteAll")

    # Monitor plugin unload events
    try:
        _script_job(_check_plugin_unload, event="PluginUnloaded")
    except:
        pass


def _tool_changed():
    """Handle tool change event"""
    if tc_motion_path_widget is None:
        return

    tool = cmds.currentCtx()
    tc_motion_path_widget.set_active_tool(tool)


def _disable():
    """Disable motion path tool and clean up viewport overrides"""
    _delete_script_jobs()

    for t in TOOLS.all():
        if cmds.currentCtx() == t:
            cmds.setToolTo('selectSuperContext')
        if cmds.contextInfo(t, exists=True):
            cmds.deleteUI(t)

    # Clean up any remaining viewport overrides
    _cleanup_viewport_overrides()

    if tc_motion_path_widget:
        tc_motion_path_widget.set_active_tool('selectSuperContext')


def _init_tool():
    """Initialize motion path tools"""
    if not cmds.contextInfo(TOOLS.EDIT, exists=True):
        cmds.tcMotionPathEditContext(TOOLS.EDIT)
    if not cmds.contextInfo(TOOLS.DRAW, exists=True):
        cmds.tcMotionPathDrawContext(TOOLS.DRAW)

    _init_script_jobs()


def _enable_motion_path( status ):
    """Enable or disable motion path functionality

    Only affects VP2 viewports. Non-VP2 viewports (Arnold, etc.) are skipped.
    """
    global _viewport_original_settings

    # Check plugin loaded before any operations
    if not _is_plugin_loaded():
        cmds.warning("Motion Path plugin is not loaded. Please load the plugin first.")
        return False

    if not status:
        _disable()
        # Disable locked mode when disabling motion path to prevent display issues
        # This ensures that when re-enabling, paths will be shown correctly
        _safe_tcMotionPathCmd(lockedMode=False)
    else:
        _init_tool()
        _refresh_selection()
        _playback_range()

    if not _safe_tcMotionPathCmd(enable=status):
        cmds.warning("Failed to enable/disable motion path. Plugin may have been unloaded.")
        return False

    # Set/clear VP2 override - ONLY for VP2 viewports
    try:
        vp2_panel_count = 0
        non_vp2_panels = []

        for panel in cmds.getPanel(type="modelPanel"):
            # Query current renderer
            try:
                current_renderer = cmds.modelEditor(panel, query=True, rendererName=True)
            except:
                continue

            # Only process VP2 viewports
            if current_renderer == "vp2Renderer":
                vp2_panel_count += 1

                if status:
                    # Enable: Save original override and set Motion Path override
                    if panel not in _viewport_original_settings:
                        try:
                            original_override = cmds.modelEditor(panel, query=True, rendererOverrideName=True)
                            _viewport_original_settings[panel] = {
                                'renderer': current_renderer,
                                'override': original_override if original_override else ""
                            }
                        except:
                            _viewport_original_settings[panel] = {
                                'renderer': current_renderer,
                                'override': ""
                            }

                    # Set Motion Path override
                    cmds.modelEditor(panel, edit=True,
                                   rendererName="vp2Renderer",
                                   rendererOverrideName="ToolchefsMotionPathOverride")
                else:
                    # Disable: Restore original override
                    if panel in _viewport_original_settings:
                        original = _viewport_original_settings[panel]
                        cmds.modelEditor(panel, edit=True,
                                       rendererName=original['renderer'],
                                       rendererOverrideName=original['override'])
                        # Clean up saved settings
                        del _viewport_original_settings[panel]
                    else:
                        # No saved settings, just clear override
                        cmds.modelEditor(panel, edit=True,
                                       rendererName="vp2Renderer",
                                       rendererOverrideName="")
            else:
                # Non-VP2 viewport - skip
                non_vp2_panels.append((panel, current_renderer))

        # Report results
        if status and vp2_panel_count == 0:
            cmds.warning("No VP2 viewports found. Motion Path requires at least one VP2 viewport. "
                        "Please switch a viewport to VP2 renderer (Viewport menu > Renderer > Viewport 2.0)")
            return False

        if status and non_vp2_panels:
            renderers = ", ".join([f"{p} ({r})" for p, r in non_vp2_panels])
            print(f"[MotionPath] Skipped {len(non_vp2_panels)} non-VP2 viewport(s): {renderers}")
            print(f"[MotionPath] Motion Path enabled on {vp2_panel_count} VP2 viewport(s)")

        # Force viewport refresh after override change
        cmds.refresh(force=True)
        return True

    except Exception as e:
        cmds.warning(f"Error setting viewport override: {e}")
        import traceback
        traceback.print_exc()
        return False


def _refresh_selection():
    """Refresh current selection"""
    selection = cmds.ls(sl=True, l=True)
    if selection:
        cmds.select(*selection, r=True)


def _cleanup_viewport_overrides():
    """Clean up all Motion Path viewport overrides and restore VP2 defaults"""
    global _viewport_original_settings

    try:
        for panel in list(_viewport_original_settings.keys()):
            try:
                original = _viewport_original_settings[panel]
                cmds.modelEditor(panel, edit=True,
                               rendererName=original['renderer'],
                               rendererOverrideName=original['override'])
            except:
                # Panel may have been deleted, just clear override
                try:
                    cmds.modelEditor(panel, edit=True,
                                   rendererName="vp2Renderer",
                                   rendererOverrideName="")
                except:
                    pass

        # Clear all saved settings
        _viewport_original_settings.clear()

    except Exception as e:
        print(f"[MotionPath] Warning: Error cleaning up viewport overrides: {e}")


def _is_plugin_loaded():
    """Check if motionPath plugin is currently loaded

    Returns:
        bool: True if plugin is loaded, False otherwise
    """
    try:
        return cmds.pluginInfo('motionPath', query=True, loaded=True)
    except:
        return False


def _safe_tcMotionPathCmd(**kwargs):
    """Safely execute tcMotionPathCmd with plugin validation

    Args:
        **kwargs: Arguments to pass to tcMotionPathCmd

    Returns:
        bool: True if command succeeded, False if plugin not loaded or command failed
    """
    if not _is_plugin_loaded():
        # Plugin not loaded - silently fail to avoid error spam
        return False

    try:
        cmds.tcMotionPathCmd(**kwargs)
        return True
    except RuntimeError as e:
        # Command failed - log but don't crash
        print(f"[MotionPath] Command failed: {e}")
        return False
    except Exception as e:
        # Unexpected error
        print(f"[MotionPath] Unexpected error: {e}")
        return False


def _get_default_value( name ):
    """Get default value from Maya option var or fallback to hardcoded default"""
    if cmds.optionVar(exists=name):
        return cmds.optionVar(q=name)
    return _default_values[name]


def _set_default_value( name, value ):
    """Set default value in Maya option var"""
    if isinstance(value, (int, bool)):
        cmds.optionVar(intValue=[name, value])
    elif isinstance(value, float):
        cmds.optionVar(floatValue=[name, value])
    elif isinstance(value, list):
        cmds.optionVar(clearArray=name)
        for v in value:
            cmds.optionVar(floatValueAppend=[name, v])


#############################################################
# UI HELPER FUNCTIONS
#############################################################

def _build_layout( vertical=True ):
    """Build a QVBoxLayout or QHBoxLayout"""
    if vertical:
        layout = QtWidgets.QVBoxLayout()
    else:
        layout = QtWidgets.QHBoxLayout()
    layout.setContentsMargins(2, 2, 2, 2)
    layout.setSpacing(2)
    return layout


def _build_button( label, callback, icon=None, tooltip=None, checkable=False ):
    """Build a QPushButton with standard settings"""
    button = QtWidgets.QPushButton(label)
    button.setCheckable(checkable)

    if icon:
        button.setIcon(QtGui.QIcon(icon))

    if tooltip:
        button.setToolTip(tooltip)

    if callback:
        button.clicked.connect(callback)

    return button


def _build_checkbox( label, callback, checked=False ):
    """Build a QCheckBox with standard settings"""
    checkbox = QtWidgets.QCheckBox(label)
    checkbox.setChecked(checked)

    if callback:
        checkbox.stateChanged.connect(callback)

    return checkbox


#############################################################
# UI WIDGETS - LineWidget
#############################################################

class LineWidget(QtWidgets.QWidget):
    """Widget with label and control on a single line"""

    def __init__( self, label, widget, parent=None ):
        super(LineWidget, self).__init__(parent)

        layout = _build_layout(False)
        self.setLayout(layout)

        label_widget = QtWidgets.QLabel(label)
        label_widget.setMinimumWidth(150)
        layout.addWidget(label_widget)
        layout.addWidget(widget)


#############################################################
# UI WIDGETS - SettingsColorButton
#############################################################

class SettingsColorButton(QtWidgets.QPushButton):
    """Color picker button for settings"""

    # Define signal - use the appropriate Signal class based on Qt version
    color_changed = Signal()

    def __init__( self, color, parent=None ):
        super(SettingsColorButton, self).__init__(parent)

        self._color = color
        # Set minimum size to ensure button is visible and clickable
        self.setMinimumSize(60, 24)
        self.setMaximumHeight(24)
        self.clicked.connect(self._on_click)
        self._update_style()

    @property
    def color( self ):
        return self._color

    @color.setter
    def color( self, value ):
        self._color = value
        self._update_style()

    def _update_style( self ):
        """Update button style to show current color"""
        # Use object name + ID selector to ensure style ONLY applies to this specific button instance
        # This completely isolates the style from all other UI elements
        object_name = f"ColorButton_{id(self)}"
        self.setObjectName(object_name)

        self.setStyleSheet(
            f"QPushButton#{object_name} {{ "
            f"background-color: rgb({self._color.red()}, {self._color.green()}, {self._color.blue()}); "
            f"border: 1px solid #555; "
            f"border-radius: 2px; "
            f"min-width: 60px; "
            f"max-width: 60px; "
            f"min-height: 24px; "
            f"max-height: 24px; "
            f"}} "
            f"QPushButton#{object_name}:hover {{ "
            f"border: 1px solid #888; "
            f"}}"
        )

    @Slot()
    def _on_click( self ):
        """Show color picker dialog"""
        color = QtWidgets.QColorDialog.getColor(self._color, self)
        if color.isValid():
            self.color = color
            self.color_changed.emit()


#############################################################
# UI WIDGETS - ToolButton
#############################################################

class ToolButton(QtWidgets.QPushButton):
    """Custom tool button with icon and checkable state"""

    def __init__( self, tool_name, parent=None ):
        super(ToolButton, self).__init__(parent)

        self._tool_name = tool_name
        self.setCheckable(True)
        self.setMaximumHeight(40)

        # Set icon if available
        icon_path = _get_icons_path() + tool_name + '.png'
        if os.path.exists(icon_path):
            self.setIcon(QtGui.QIcon(icon_path))
            self.setIconSize(QtCore.QSize(32, 32))

        # Set tooltip
        tooltip = TOOLS.tooltip(tool_name)
        if tooltip:
            self.setToolTip(tooltip)

        self.clicked.connect(self._on_click)

    @Slot()
    def _on_click( self ):
        """Handle button click"""
        if self.isChecked():
            cmds.setToolTo(self._tool_name)
        else:
            cmds.setToolTo('selectSuperContext')


#############################################################
# UI WIDGETS - ToolButtons Container
#############################################################

class ToolButtons(QtWidgets.QWidget):
    """Container for tool buttons"""

    def __init__( self, parent=None ):
        super(ToolButtons, self).__init__(parent)

        layout = _build_layout(False)
        self.setLayout(layout)

        self._buttons = []
        for tool_name in TOOLS.all():
            button = ToolButton(tool_name, parent=self)
            self._buttons.append(button)
            layout.addWidget(button)

    def set_active_tool( self, tool_name ):
        """Set active tool button"""
        for button in self._buttons:
            button.setChecked(button._tool_name == tool_name)


#############################################################
# UI WIDGETS - EditPathWidget (Main Edit Tab)
#############################################################

class EditPathWidget(QtWidgets.QWidget):
    """Main edit path widget"""

    # Define signal
    enabled = Signal(bool)

    def __init__( self, parent=None ):
        super(EditPathWidget, self).__init__(parent)

        self._main_layout = _build_layout(True)
        self.setLayout(self._main_layout)

        # Enable/Disable button
        self._enable_button = _build_button(
            "Enable Motion Path",
            self._toggle_enable,
            checkable=True
        )
        self._main_layout.addWidget(self._enable_button)

        # Tool buttons
        self._edit_buttons = ToolButtons(parent=self)
        self._main_layout.addWidget(self._edit_buttons)
        self._edit_buttons.setEnabled(False)

        # Separator
        line = QtWidgets.QFrame()
        line.setFrameShape(QtWidgets.QFrame.HLine)
        line.setFrameShadow(QtWidgets.QFrame.Sunken)
        self._main_layout.addWidget(line)

        # Display options
        self._create_display_options()

        # Advanced options
        self._create_advanced_options()

        # Frame range
        self._create_frame_range()

        # Buffer buttons
        self._create_buffer_buttons()

        # Spacer
        self._main_layout.addWidget(QtWidgets.QWidget(), stretch=1)

        # Initialize
        self._initialize_widgets()
        self._connect_signals()

    def _create_display_options( self ):
        """Create display option checkboxes"""
        group = QtWidgets.QGroupBox("Display Options")
        layout = _build_layout(True)
        group.setLayout(layout)

        self._show_path = _build_checkbox(
            "Show Path",
            self._show_path_changed,
            _get_default_value(StaticLabels.SHOW_PATH)
        )
        layout.addWidget(self._show_path)

        self._show_keyframes = _build_checkbox(
            "Show Keyframes",
            self._show_keyframes_changed,
            _get_default_value(StaticLabels.SHOW_KEYFRAMES)
        )
        layout.addWidget(self._show_keyframes)

        self._show_rotation = _build_checkbox(
            "Show Rotation Keyframes",
            self._show_rotation_changed,
            _get_default_value(StaticLabels.SHOW_ROTATION)
        )
        layout.addWidget(self._show_rotation)

        self._show_tangents = _build_checkbox(
            "Show Tangents",
            self._show_tangents_changed,
            _get_default_value(StaticLabels.SHOW_TANGENTS)
        )
        layout.addWidget(self._show_tangents)

        self._alternate_color = _build_checkbox(
            "Alternate Frame Colors",
            self._alternate_color_changed,
            _get_default_value(StaticLabels.ALTERNATE_COLOR)
        )
        layout.addWidget(self._alternate_color)

        self._use_pivots = _build_checkbox(
            "Use Pivots",
            self._use_pivots_changed,
            _get_default_value(StaticLabels.USEPIVOTS)
        )
        layout.addWidget(self._use_pivots)

        self._show_knumber = _build_checkbox(
            "Show Key Numbers",
            self._show_knumber_changed,
            _get_default_value(StaticLabels.SHOW_KNUMBER)
        )
        layout.addWidget(self._show_knumber)

        self._show_fnumber = _build_checkbox(
            "Show Frame Numbers",
            self._show_fnumber_changed,
            _get_default_value(StaticLabels.SHOW_FNUMBER)
        )
        layout.addWidget(self._show_fnumber)

        self._main_layout.addWidget(group)

    def _create_advanced_options( self ):
        """Create advanced option controls"""
        group = QtWidgets.QGroupBox("Advanced Options")
        layout = _build_layout(True)
        group.setLayout(layout)

        # Draw Mode
        draw_mode_widget = QtWidgets.QWidget()
        draw_mode_layout = _build_layout(False)
        draw_mode_widget.setLayout(draw_mode_layout)
        draw_mode_layout.addWidget(QtWidgets.QLabel("Draw Mode:"))
        self._draw_mode = QtWidgets.QComboBox()
        self._draw_mode.addItem("World Space", 0)
        self._draw_mode.addItem("Camera Space", 1)
        self._draw_mode.setCurrentIndex(0)
        draw_mode_layout.addWidget(self._draw_mode)
        layout.addWidget(draw_mode_widget)

        # Stroke Mode
        stroke_mode_widget = QtWidgets.QWidget()
        stroke_mode_layout = _build_layout(False)
        stroke_mode_widget.setLayout(stroke_mode_layout)
        stroke_mode_layout.addWidget(QtWidgets.QLabel("Stroke Mode:"))
        self._stroke_mode = QtWidgets.QComboBox()
        self._stroke_mode.addItem("Closest", 0)
        self._stroke_mode.addItem("Spread", 1)
        self._stroke_mode.setCurrentIndex(0)
        stroke_mode_layout.addWidget(self._stroke_mode)
        layout.addWidget(stroke_mode_widget)

        # Locked Mode
        self._locked_mode = _build_checkbox(
            "Locked Mode",
            self._locked_mode_changed,
            False
        )
        layout.addWidget(self._locked_mode)

        self._locked_mode_interactive = _build_checkbox(
            "Locked Mode Interactive",
            self._locked_mode_interactive_changed,
            False
        )
        layout.addWidget(self._locked_mode_interactive)

        self._main_layout.addWidget(group)

    def _create_frame_range( self ):
        """Create frame range controls"""
        group = QtWidgets.QGroupBox("Frame Range")
        layout = _build_layout(True)
        group.setLayout(layout)

        # Frames before
        before_widget = QtWidgets.QWidget()
        before_layout = _build_layout(False)
        before_widget.setLayout(before_layout)
        before_layout.addWidget(QtWidgets.QLabel("Frames Before:"))
        self._frames_before = QtWidgets.QSpinBox()
        self._frames_before.setMinimum(0)
        self._frames_before.setMaximum(1000)
        self._frames_before.setValue(_get_default_value(StaticLabels.FRAMES_BEFORE))
        before_layout.addWidget(self._frames_before)
        layout.addWidget(before_widget)

        # Frames after
        after_widget = QtWidgets.QWidget()
        after_layout = _build_layout(False)
        after_widget.setLayout(after_layout)
        after_layout.addWidget(QtWidgets.QLabel("Frames After:"))
        self._frames_after = QtWidgets.QSpinBox()
        self._frames_after.setMinimum(0)
        self._frames_after.setMaximum(1000)
        self._frames_after.setValue(_get_default_value(StaticLabels.FRAMES_AFTER))
        after_layout.addWidget(self._frames_after)
        layout.addWidget(after_widget)

        # Time delta
        time_widget = QtWidgets.QWidget()
        time_layout = _build_layout(False)
        time_widget.setLayout(time_layout)
        time_layout.addWidget(QtWidgets.QLabel("Time Delta:"))
        self._time_delta = QtWidgets.QDoubleSpinBox()
        self._time_delta.setMinimum(0.01)
        self._time_delta.setMaximum(10.0)
        self._time_delta.setSingleStep(0.1)
        self._time_delta.setValue(_get_default_value(StaticLabels.TIME_DELTA))
        time_layout.addWidget(self._time_delta)
        layout.addWidget(time_widget)

        # Separator
        separator = QtWidgets.QFrame()
        separator.setFrameShape(QtWidgets.QFrame.HLine)
        separator.setFrameShadow(QtWidgets.QFrame.Sunken)
        layout.addWidget(separator)

        # Click mode label
        click_label = QtWidgets.QLabel("Click Mode (Experimental):")
        click_label.setStyleSheet("font-weight: bold; color: #888; margin-top: 5px;")
        layout.addWidget(click_label)

        # Draw keyframe count (number of keyframes to ADD, not including start)
        keyframe_count_widget = QtWidgets.QWidget()
        keyframe_count_layout = _build_layout(False)
        keyframe_count_widget.setLayout(keyframe_count_layout)
        keyframe_count_layout.addWidget(QtWidgets.QLabel("Add Keyframes:"))
        self._draw_keyframe_count = QtWidgets.QSpinBox()
        self._draw_keyframe_count.setMinimum(1)
        self._draw_keyframe_count.setMaximum(10)
        self._draw_keyframe_count.setValue(_get_default_value(StaticLabels.DRAW_KEYFRAME_COUNT))
        keyframe_count_layout.addWidget(self._draw_keyframe_count)
        layout.addWidget(keyframe_count_widget)

        # Click frame interval (for drawing keyframes)
        click_widget = QtWidgets.QWidget()
        click_layout = _build_layout(False)
        click_widget.setLayout(click_layout)
        click_layout.addWidget(QtWidgets.QLabel("Click Interval:"))
        self._frame_delta = QtWidgets.QSpinBox()
        self._frame_delta.setRange(1, 100)
        self._frame_delta.setValue(_get_default_value(StaticLabels.FRAME_DELTA))
        self._frame_delta.setToolTip(
            "Frame interval between drawn keyframes.\n"
            "Example: Value=5, Add 3 keyframes = keyframes at +5, +10, +15 from start"
        )
        click_layout.addWidget(self._frame_delta)
        layout.addWidget(click_widget)

        # Info
        info_label = QtWidgets.QLabel(
            "Click mode: Middle-Click adds keyframes at fixed intervals"
        )
        info_label.setWordWrap(True)
        info_label.setStyleSheet("color: #666; font-size: 9px; margin-bottom: 5px;")
        layout.addWidget(info_label)

        self._main_layout.addWidget(group)

    def _create_buffer_buttons( self ):
        """Create buffer path buttons - removed, now in Buffered Paths tab"""
        pass

    def _initialize_widgets( self ):
        """Initialize widget values and send to C++ plugin"""
        try:
            # Send all display options to C++ plugin
            _safe_tcMotionPathCmd(showPath=_get_default_value(StaticLabels.SHOW_PATH))
            _safe_tcMotionPathCmd(showKeyFrames=_get_default_value(StaticLabels.SHOW_KEYFRAMES))
            _safe_tcMotionPathCmd(showRotationKeyFrames=_get_default_value(StaticLabels.SHOW_ROTATION))
            _safe_tcMotionPathCmd(showTangents=_get_default_value(StaticLabels.SHOW_TANGENTS))
            _safe_tcMotionPathCmd(alternatingFrames=_get_default_value(StaticLabels.ALTERNATE_COLOR))
            _safe_tcMotionPathCmd(usePivots=_get_default_value(StaticLabels.USEPIVOTS))
            _safe_tcMotionPathCmd(showKeyFrameNumbers=_get_default_value(StaticLabels.SHOW_KNUMBER))
            _safe_tcMotionPathCmd(showFrameNumbers=_get_default_value(StaticLabels.SHOW_FNUMBER))

            # Send frame range values to C++ plugin
            _safe_tcMotionPathCmd(framesBefore=_get_default_value(StaticLabels.FRAMES_BEFORE))
            _safe_tcMotionPathCmd(framesAfter=_get_default_value(StaticLabels.FRAMES_AFTER))
            _safe_tcMotionPathCmd(frameInterval=_get_default_value(StaticLabels.FRAME_DELTA))
            _safe_tcMotionPathCmd(drawKeyframeCount=_get_default_value(StaticLabels.DRAW_KEYFRAME_COUNT))
            _safe_tcMotionPathCmd(drawTimeInterval=_get_default_value(StaticLabels.TIME_DELTA))

            # Send draw mode and stroke mode
            _safe_tcMotionPathCmd(drawMode=0)  # World Space by default
            _safe_tcMotionPathCmd(strokeMode=0)  # Closest by default
        except Exception as e:
            print(f"Warning: Error initializing widget values: {e}")
            traceback.print_exc()

    def _connect_signals( self ):
        """Connect widget signals"""
        self._frames_before.valueChanged.connect(self._frames_before_changed)
        self._frames_after.valueChanged.connect(self._frames_after_changed)
        self._frame_delta.valueChanged.connect(self._frame_delta_changed)
        self._draw_keyframe_count.valueChanged.connect(self._draw_keyframe_count_changed)
        self._time_delta.valueChanged.connect(self._time_delta_changed)
        self._draw_mode.currentIndexChanged.connect(self._draw_mode_changed)
        self._stroke_mode.currentIndexChanged.connect(self._stroke_mode_changed)

    @Slot()
    def _toggle_enable( self ):
        """Toggle motion path enable state"""
        enabled = self._enable_button.isChecked()
        self._edit_buttons.setEnabled(enabled)

        # When disabling, also uncheck locked mode to prevent display issues on re-enable
        if not enabled and hasattr(self, '_locked_mode'):
            self._locked_mode.setChecked(False)

        # Try to enable/disable motion path
        success = _enable_motion_path(enabled)

        # If enable failed (plugin not loaded), uncheck the button
        if enabled and not success:
            self._enable_button.setChecked(False)
            self._edit_buttons.setEnabled(False)
            return

        self.enabled.emit(enabled)

    @Slot(int)
    def _show_path_changed( self, state ):
        """Handle show path checkbox change"""
        value = bool(state == 2)  # Qt.Checked = 2 in all Qt versions
        _safe_tcMotionPathCmd(showPath=value)
        _set_default_value(StaticLabels.SHOW_PATH, value)
        cmds.refresh()

    @Slot(int)
    def _show_keyframes_changed( self, state ):
        """Handle show keyframes checkbox change"""
        value = bool(state == 2)  # Qt.Checked = 2 in all Qt versions
        _safe_tcMotionPathCmd(showKeyFrames=value)
        _set_default_value(StaticLabels.SHOW_KEYFRAMES, value)
        cmds.refresh()

    @Slot(int)
    def _show_rotation_changed( self, state ):
        """Handle show rotation checkbox change"""
        value = bool(state == 2)  # Qt.Checked = 2 in all Qt versions
        _safe_tcMotionPathCmd(showRotationKeyFrames=value)
        _set_default_value(StaticLabels.SHOW_ROTATION, value)
        cmds.refresh()

    @Slot(int)
    def _show_tangents_changed( self, state ):
        """Handle show tangents checkbox change"""
        value = bool(state == 2)  # Qt.Checked = 2 in all Qt versions
        _safe_tcMotionPathCmd(showTangents=value)
        _set_default_value(StaticLabels.SHOW_TANGENTS, value)
        cmds.refresh()

    @Slot(int)
    def _alternate_color_changed( self, state ):
        """Handle alternate color checkbox change"""
        value = bool(state == 2)  # Qt.Checked = 2 in all Qt versions
        _safe_tcMotionPathCmd(alternatingFrames=value)
        _set_default_value(StaticLabels.ALTERNATE_COLOR, value)
        cmds.refresh()

    @Slot(int)
    def _use_pivots_changed( self, state ):
        """Handle use pivots checkbox change"""
        value = bool(state == 2)  # Qt.Checked = 2 in all Qt versions
        _safe_tcMotionPathCmd(usePivots=value)
        _set_default_value(StaticLabels.USEPIVOTS, value)
        cmds.refresh()

    @Slot(int)
    def _show_knumber_changed( self, state ):
        """Handle show key numbers checkbox change"""
        value = bool(state == 2)  # Qt.Checked = 2 in all Qt versions
        _safe_tcMotionPathCmd(showKeyFrameNumbers=value)
        _set_default_value(StaticLabels.SHOW_KNUMBER, value)
        cmds.refresh()

    @Slot(int)
    def _show_fnumber_changed( self, state ):
        """Handle show frame numbers checkbox change"""
        value = bool(state == 2)  # Qt.Checked = 2 in all Qt versions
        _safe_tcMotionPathCmd(showFrameNumbers=value)
        _set_default_value(StaticLabels.SHOW_FNUMBER, value)
        cmds.refresh()

    @Slot(int)
    def _frames_before_changed( self, value ):
        """Handle frames before value change - updates in real-time"""
        try:
            _safe_tcMotionPathCmd(framesBefore=value)
            _set_default_value(StaticLabels.FRAMES_BEFORE, value)
            cmds.refresh()
        except Exception as e:
            print(f"Error updating frames before: {e}")
            traceback.print_exc()

    @Slot(int)
    def _frames_after_changed( self, value ):
        """Handle frames after value change - updates in real-time"""
        try:
            _safe_tcMotionPathCmd(framesAfter=value)
            _set_default_value(StaticLabels.FRAMES_AFTER, value)
            cmds.refresh()
        except Exception as e:
            print(f"Error updating frames after: {e}")
            traceback.print_exc()

    @Slot(int)
    def _frame_delta_changed( self, value ):
        """Handle frame delta value change - updates in real-time"""
        try:
            _safe_tcMotionPathCmd(frameInterval=value)
            _set_default_value(StaticLabels.FRAME_DELTA, value)
            cmds.refresh()
        except Exception as e:
            print(f"Error updating frame interval: {e}")
            traceback.print_exc()

    @Slot(int)
    def _draw_keyframe_count_changed( self, value ):
        """Handle draw keyframe count value change - updates preview in real-time"""
        try:
            _safe_tcMotionPathCmd(drawKeyframeCount=value)
            _set_default_value(StaticLabels.DRAW_KEYFRAME_COUNT, value)
            # No refresh needed - preview updates automatically during next drag
        except Exception as e:
            print(f"Error updating draw keyframe count: {e}")
            traceback.print_exc()

    @Slot(float)
    def _time_delta_changed( self, value ):
        """Handle time delta value change - updates path smoothness in real-time"""
        try:
            _safe_tcMotionPathCmd(drawTimeInterval=value)
            _set_default_value(StaticLabels.TIME_DELTA, value)
            cmds.refresh()
        except Exception as e:
            print(f"Error updating time delta: {e}")
            traceback.print_exc()

    @Slot()
    def _add_buffer_path( self ):
        """Add current path to buffer - delegate to MotionPathBufferPaths"""
        if tc_motion_path_widget:
            tc_motion_path_widget.mbp.add_buffer_paths()

    @Slot()
    def _clear_buffer_paths( self ):
        """Clear all buffer paths - delegate to MotionPathBufferPaths"""
        if tc_motion_path_widget:
            tc_motion_path_widget.mbp.empty_buffer_paths()

    @Slot(int)
    def _draw_mode_changed( self, index ):
        """Handle draw mode change"""
        mode = self._draw_mode.itemData(index)
        _safe_tcMotionPathCmd(drawMode=mode)
        cmds.refresh()

    @Slot(int)
    def _stroke_mode_changed( self, index ):
        """Handle stroke mode change"""
        mode = self._stroke_mode.itemData(index)
        _safe_tcMotionPathCmd(strokeMode=mode)
        cmds.refresh()

    @Slot(int)
    def _locked_mode_changed( self, state ):
        """Handle locked mode checkbox change"""
        value = bool(state == 2)  # Qt.Checked = 2 in all Qt versions
        _safe_tcMotionPathCmd(lockedMode=value)
        cmds.refresh()

    @Slot(int)
    def _locked_mode_interactive_changed( self, state ):
        """Handle locked mode interactive checkbox change"""
        value = bool(state == 2)  # Qt.Checked = 2 in all Qt versions
        _safe_tcMotionPathCmd(lockedModeInteractive=value)
        cmds.refresh()


#############################################################
# UI WIDGETS - MotionPathBufferPaths
#############################################################

class MotionPathBufferPaths(QtWidgets.QWidget):
    """Buffer paths management widget"""

    def __init__( self, parent=None ):
        super(MotionPathBufferPaths, self).__init__(parent)

        self._main_layout = _build_layout(True)
        self.setLayout(self._main_layout)

        # Track buffer paths: list of dicts with {name, objName}
        self._buffer_data = []

        # List widget with single-click selection + Shift for multi-select
        self._buffer_list = QtWidgets.QListWidget()
        # ExtendedSelection: single-click selects one, Shift/Ctrl for multi-select
        self._buffer_list.setSelectionMode(QtWidgets.QAbstractItemView.ExtendedSelection)
        self._main_layout.addWidget(self._buffer_list)

        # Buttons
        button_layout = _build_layout(False)

        self._add_button = _build_button(
            "Add Current",
            self.add_buffer_paths
        )
        button_layout.addWidget(self._add_button)

        self._clear_all_button = _build_button(
            "Clear All",
            self._clear_all
        )
        button_layout.addWidget(self._clear_all_button)

        self._main_layout.addLayout(button_layout)

        # Second row buttons
        button_layout2 = _build_layout(False)

        self._remove_button = _build_button(
            "Remove Selected",
            self._remove_selected
        )
        button_layout2.addWidget(self._remove_button)

        self._convert_button = _build_button(
            "Convert to Curve",
            self._convert_selected
        )
        button_layout2.addWidget(self._convert_button)

        self._main_layout.addLayout(button_layout2)

        # Connect signals
        self._buffer_list.itemSelectionChanged.connect(self._selection_changed)

    def add_buffer_paths( self ):
        """Add buffer paths for currently selected objects"""
        try:
            # Get current Maya selection
            selection = cmds.ls(selection=True, long=False, transforms=True)
            if not selection:
                QtWidgets.QMessageBox.warning(
                    self,
                    "No Selection",
                    "Please select one or more objects to add buffer paths."
                )
                return

            # Add buffer paths in C++
            _safe_tcMotionPathCmd(addBufferPaths=True)

            # Add each object to UI list with naming
            for obj in selection:
                item_name = f"{obj}_BufferPath"
                self._buffer_data.append({
                    'name': item_name,
                    'objName': obj
                })
                self._buffer_list.addItem(item_name)

            cmds.refresh()

        except Exception as e:
            print(f"Error adding buffer paths: {e}")
            traceback.print_exc()
            QtWidgets.QMessageBox.critical(
                self,
                "Error",
                f"Failed to add buffer paths:\n{e}"
            )

    @Slot()
    def _selection_changed( self ):
        """Handle selection change"""
        selected_indices = [self._buffer_list.row(item)
                            for item in self._buffer_list.selectedItems()]
        try:
            # First deselect all
            for i in range(self._buffer_list.count()):
                _safe_tcMotionPathCmd(deselectBufferPathAtIndex=i)
            # Then select the chosen ones
            for index in selected_indices:
                _safe_tcMotionPathCmd(selectBufferPathAtIndex=index)
            cmds.refresh()
        except Exception as e:
            print(f"Error selecting buffer paths: {e}")

    @Slot()
    def _remove_selected( self ):
        """Remove selected buffer paths"""
        selected_items = self._buffer_list.selectedItems()
        if not selected_items:
            return

        try:
            # Get indices before any deletion
            indices_to_remove = sorted([self._buffer_list.row(item) for item in selected_items], reverse=True)

            # Delete from C++ and UI in reverse order
            for index in indices_to_remove:
                _safe_tcMotionPathCmd(deleteBufferPathAtIndex=index)
                self._buffer_list.takeItem(index)

            # Rebuild _buffer_data by querying remaining items
            self._buffer_data = [
                {'name': self._buffer_list.item(i).text(),
                 'objName': self._buffer_list.item(i).text().replace('_BufferPath', '')}
                for i in range(self._buffer_list.count())
            ]

            cmds.refresh()
        except Exception as e:
            print(f"Error removing buffer paths: {e}")
            traceback.print_exc()

    @Slot()
    def _clear_all( self ):
        """Clear all buffer paths"""
        self.empty_buffer_paths()

    @Slot()
    def _convert_selected( self ):
        """Convert selected buffer paths to NURBS curves"""
        selected_items = self._buffer_list.selectedItems()
        if not selected_items:
            QtWidgets.QMessageBox.information(
                self,
                "No Selection",
                "Please select buffer path(s) to convert"
            )
            return

        # Get indices and names for feedback
        conversions = []
        for item in selected_items:
            index = self._buffer_list.row(item)
            object_name = item.text()
            conversions.append((index, object_name))

        try:
            # Convert each selected buffer path
            converted_names = []
            for index, object_name in conversions:
                _safe_tcMotionPathCmd(convertBufferPath=index)
                converted_names.append(f"{object_name}_Buffer_Path")

            cmds.refresh()

            # Show success message with curve names
            message = f"Successfully converted {len(conversions)} buffer path(s):\n\n"
            message += "\n".join(converted_names)

            QtWidgets.QMessageBox.information(
                self,
                "Success",
                message
            )
        except Exception as e:
            print(f"Error converting buffer paths: {e}")
            traceback.print_exc()
            QtWidgets.QMessageBox.warning(
                self,
                "Error",
                f"Failed to convert buffer paths:\n{e}"
            )

    def empty_buffer_paths( self ):
        """Empty all buffer paths"""
        try:
            _safe_tcMotionPathCmd(deleteAllBufferPaths=True)
            self._buffer_list.clear()
            self._buffer_data.clear()
            cmds.refresh()
        except Exception as e:
            print(f"Error clearing buffer paths: {e}")
            traceback.print_exc()

    def deselect_all( self ):
        """Deselect all items"""
        self._buffer_list.clearSelection()


#############################################################
# UI WIDGETS - MotionPathWidgetSettings
#############################################################

class MotionPathWidgetSettings(QtWidgets.QWidget):
    """Settings widget for motion path"""

    def __init__( self, parent=None ):
        super(MotionPathWidgetSettings, self).__init__(parent)

        self._main_layout = _build_layout(True)
        self.setLayout(self._main_layout)

        # Size settings
        self._create_size_settings()

        # Color settings
        self._create_color_settings()

        # Spacer
        self._main_layout.addWidget(QtWidgets.QWidget(), stretch=1)

        # Initialize and connect
        self._initialize_widgets()
        self._connect_signals()

    def _create_size_settings( self ):
        """Create size setting controls"""
        group = QtWidgets.QGroupBox("Size Settings")
        layout = _build_layout(True)
        group.setLayout(layout)

        # Path size
        self._path_size = QtWidgets.QLineEdit()
        self._path_size.setValidator(QtGui.QDoubleValidator(0.1, 100.0, 2))
        line_widget = LineWidget("Path Size:", self._path_size, parent=self)
        layout.addWidget(line_widget)

        # Frame size
        self._frame_size = QtWidgets.QLineEdit()
        self._frame_size.setValidator(QtGui.QDoubleValidator(0.1, 100.0, 2))
        line_widget = LineWidget("Frame Size:", self._frame_size, parent=self)
        layout.addWidget(line_widget)

        # Keyframe label size
        self._keyframe_label_size = QtWidgets.QDoubleSpinBox()
        self._keyframe_label_size.setMinimum(0.1)
        self._keyframe_label_size.setMaximum(15.0)
        self._keyframe_label_size.setSingleStep(0.1)
        self._keyframe_label_size.setDecimals(1)
        line_widget = LineWidget("Keyframe Label Size:", self._keyframe_label_size, parent=self)
        layout.addWidget(line_widget)

        # Frame label size
        self._frame_label_size = QtWidgets.QDoubleSpinBox()
        self._frame_label_size.setMinimum(0.1)
        self._frame_label_size.setMaximum(15.0)
        self._frame_label_size.setSingleStep(0.1)
        self._frame_label_size.setDecimals(1)
        line_widget = LineWidget("Frame Label Size:", self._frame_label_size, parent=self)
        layout.addWidget(line_widget)

        self._main_layout.addWidget(group)

    def _create_color_settings( self ):
        """Create color setting controls"""
        group = QtWidgets.QGroupBox("Color Settings")
        layout = _build_layout(True)
        group.setLayout(layout)

        # Path color
        self._path_color = SettingsColorButton(
            QtGui.QColor(0, 178, 238, 255),
            parent=self
        )
        line_widget = LineWidget("Path Color:", self._path_color, parent=self)
        layout.addWidget(line_widget)

        # Current frame color
        self._cframe_color = SettingsColorButton(
            QtGui.QColor(255, 255, 0, 255),
            parent=self
        )
        line_widget = LineWidget("Current Frame Color:", self._cframe_color, parent=self)
        layout.addWidget(line_widget)

        # Tangent color
        self._tangent_color = SettingsColorButton(
            QtGui.QColor(139, 105, 105, 255),
            parent=self
        )
        line_widget = LineWidget("Tangent Color:", self._tangent_color, parent=self)
        layout.addWidget(line_widget)

        # Broken tangent color
        self._broken_tangent_color = SettingsColorButton(
            QtGui.QColor(139, 69, 19, 255),
            parent=self
        )
        line_widget = LineWidget("Broken Tangent Color:",
                                 self._broken_tangent_color, parent=self)
        layout.addWidget(line_widget)

        # Buffer path color
        self._buffer_path_color = SettingsColorButton(
            QtGui.QColor(51, 51, 51, 255),
            parent=self
        )
        line_widget = LineWidget("Buffer Path Color:",
                                 self._buffer_path_color, parent=self)
        layout.addWidget(line_widget)

        # Weighted path color
        self._weighted_path_color = SettingsColorButton(
            QtGui.QColor(255, 0, 0, 255),
            parent=self
        )
        line_widget = LineWidget("Weighted Path Color:",
                                 self._weighted_path_color, parent=self)
        layout.addWidget(line_widget)

        # Weighted tangent color
        self._weighted_tangent_color = SettingsColorButton(
            QtGui.QColor(139, 0, 0, 255),
            parent=self
        )
        line_widget = LineWidget("Weighted Path Tangent Color:",
                                 self._weighted_tangent_color, parent=self)
        layout.addWidget(line_widget)

        # Frame number color
        self._frame_number_color = SettingsColorButton(
            QtGui.QColor(25, 25, 25, 255),
            parent=self
        )
        line_widget = LineWidget("Frame Number Color:",
                                 self._frame_number_color, parent=self)
        layout.addWidget(line_widget)

        # Keyframe number color
        self._keyframe_number_color = SettingsColorButton(
            QtGui.QColor(255, 255, 0, 255),
            parent=self
        )
        line_widget = LineWidget("Keyframe Number Color:",
                                 self._keyframe_number_color, parent=self)
        layout.addWidget(line_widget)

        self._main_layout.addWidget(group)

    def _initialize_color_widget( self, widget, g_var_name, cmd_arg_name ):
        """Initialize a color widget from saved settings"""
        color = _get_default_value(g_var_name)
        widget.color = QtGui.QColor(color[0], color[1], color[2], 255)
        c_float = [float(c) / 255.0 for c in color]
        _safe_tcMotionPathCmd(**{cmd_arg_name: [c_float[0], c_float[1], c_float[2]]})

    def _initialize_widgets( self ):
        """Initialize all widgets"""
        # Path size
        ps = _get_default_value(StaticLabels.PATH_SIZE)
        self._path_size.setText(str(ps))
        _safe_tcMotionPathCmd(pathSize=ps)

        # Frame size
        fs = _get_default_value(StaticLabels.FRAME_SIZE)
        self._frame_size.setText(str(fs))
        _safe_tcMotionPathCmd(frameSize=fs)

        # Keyframe label size
        kls = _get_default_value(StaticLabels.KEYFRAME_LABEL_SIZE)
        self._keyframe_label_size.setValue(kls)
        _safe_tcMotionPathCmd(keyframeLabelSize=kls)

        # Frame label size
        fls = _get_default_value(StaticLabels.FRAME_LABEL_SIZE)
        self._frame_label_size.setValue(fls)
        _safe_tcMotionPathCmd(frameLabelSize=fls)

        # Colors
        self._initialize_color_widget(self._path_color,
                                      StaticLabels.PATH_COLOR, 'pathColor')
        self._initialize_color_widget(self._weighted_path_color,
                                      StaticLabels.WPATH_COLOR, 'weightedPathColor')
        self._initialize_color_widget(self._weighted_tangent_color,
                                      StaticLabels.WTANGENT_COLOR,
                                      'weightedPathTangentColor')
        self._initialize_color_widget(self._cframe_color,
                                      StaticLabels.CFRAME_COLOR, 'currentFrameColor')
        self._initialize_color_widget(self._tangent_color,
                                      StaticLabels.TANGENT_COLOR, 'tangentColor')
        self._initialize_color_widget(self._broken_tangent_color,
                                      StaticLabels.BTANGENT_COLOR, 'brokenTangentColor')
        self._initialize_color_widget(self._buffer_path_color,
                                      StaticLabels.BPATH_COLOR, 'bufferPathColor')
        self._initialize_color_widget(self._frame_number_color,
                                      StaticLabels.FNUMBER_COLOR, 'frameNumberColor')
        self._initialize_color_widget(self._keyframe_number_color,
                                      StaticLabels.KEYFRAME_NUMBER_COLOR, 'keyframeNumberColor')

        cmds.refresh()

    def _connect_signals( self ):
        """Connect widget signals"""
        self._frame_size.editingFinished.connect(self._frame_size_changed)
        self._path_size.editingFinished.connect(self._path_size_changed)
        self._keyframe_label_size.valueChanged.connect(self._keyframe_label_size_changed)
        self._frame_label_size.valueChanged.connect(self._frame_label_size_changed)

        # Color signals - using lambda to reduce code duplication while maintaining clarity
        self._path_color.color_changed.connect(
            lambda: self._widget_color_changed(self._path_color.color,
                                               StaticLabels.PATH_COLOR, 'pathColor'))
        self._weighted_path_color.color_changed.connect(
            lambda: self._widget_color_changed(self._weighted_path_color.color,
                                               StaticLabels.WPATH_COLOR, 'weightedPathColor'))
        self._weighted_tangent_color.color_changed.connect(
            lambda: self._widget_color_changed(self._weighted_tangent_color.color,
                                               StaticLabels.WTANGENT_COLOR, 'weightedPathTangentColor'))
        self._cframe_color.color_changed.connect(
            lambda: self._widget_color_changed(self._cframe_color.color,
                                               StaticLabels.CFRAME_COLOR, 'currentFrameColor'))
        self._tangent_color.color_changed.connect(
            lambda: self._widget_color_changed(self._tangent_color.color,
                                               StaticLabels.TANGENT_COLOR, 'tangentColor'))
        self._broken_tangent_color.color_changed.connect(
            lambda: self._widget_color_changed(self._broken_tangent_color.color,
                                               StaticLabels.BTANGENT_COLOR, 'brokenTangentColor'))
        self._buffer_path_color.color_changed.connect(
            lambda: self._widget_color_changed(self._buffer_path_color.color,
                                               StaticLabels.BPATH_COLOR, 'bufferPathColor'))
        self._frame_number_color.color_changed.connect(
            lambda: self._widget_color_changed(self._frame_number_color.color,
                                               StaticLabels.FNUMBER_COLOR, 'frameNumberColor'))
        self._keyframe_number_color.color_changed.connect(
            lambda: self._widget_color_changed(self._keyframe_number_color.color,
                                               StaticLabels.KEYFRAME_NUMBER_COLOR, 'keyframeNumberColor'))

    @Slot()
    def _frame_size_changed( self ):
        """Handle frame size change"""
        value = float(self._frame_size.text())
        _safe_tcMotionPathCmd(frameSize=value)
        _set_default_value(StaticLabels.FRAME_SIZE, value)
        cmds.refresh()

    @Slot()
    def _path_size_changed( self ):
        """Handle path size change"""
        value = float(self._path_size.text())
        _safe_tcMotionPathCmd(pathSize=value)
        _set_default_value(StaticLabels.PATH_SIZE, value)
        cmds.refresh()

    @Slot(float)
    def _keyframe_label_size_changed( self, value ):
        """Handle keyframe label size change - updates in real-time"""
        try:
            _safe_tcMotionPathCmd(keyframeLabelSize=value)
            _set_default_value(StaticLabels.KEYFRAME_LABEL_SIZE, value)
            cmds.refresh()
        except Exception as e:
            print(f"Error updating keyframe label size: {e}")
            traceback.print_exc()

    @Slot(float)
    def _frame_label_size_changed( self, value ):
        """Handle frame label size change - updates in real-time"""
        try:
            _safe_tcMotionPathCmd(frameLabelSize=value)
            _set_default_value(StaticLabels.FRAME_LABEL_SIZE, value)
            cmds.refresh()
        except Exception as e:
            print(f"Error updating frame label size: {e}")
            traceback.print_exc()

    def _widget_color_changed( self, color, g_var_name, cmd_arg_name ):
        """Handle color widget change - centralized color update logic"""
        r, g, b = color.red(), color.green(), color.blue()
        _safe_tcMotionPathCmd(**{cmd_arg_name: [r / 255.0, g / 255.0, b / 255.0]})
        _set_default_value(g_var_name, [r, g, b])
        cmds.refresh()

    # Note: Individual color change handlers removed - now using lambda connections in _connect_signals()
    # This reduces code duplication from 9 nearly-identical methods to 0, improving maintainability


#############################################################
# MAIN WIDGET - MotionPathWidget
#############################################################

class MotionPathWidget(MayaQWidgetDockableMixin, QtWidgets.QWidget):
    """Main motion path widget"""

    def __init__( self, parent=None ):
        super(MotionPathWidget, self).__init__(parent)

        self.setWindowTitle("Motion Path")
        self.setObjectName("ToolchefsMotionPath")

        self._main_layout = _build_layout(True)
        self.setLayout(self._main_layout)

        # Create tab widget
        tab_bar = QtWidgets.QTabBar(self)
        tab_bar.setStyleSheet("QTabBar::tab{width:120px;}")
        tab_bar.setExpanding(True)

        self.tabWidget = QtWidgets.QTabWidget(self)
        self.tabWidget.setTabBar(tab_bar)
        self._main_layout.addWidget(self.tabWidget)

        # Edit tab
        scroll_area1 = QtWidgets.QScrollArea(self)
        scroll_area1.setWidgetResizable(True)
        self.mws = EditPathWidget()
        scroll_area1.setWidget(self.mws)
        self.tabWidget.addTab(scroll_area1, "Edit")

        # Buffer paths tab
        scroll_area3 = QtWidgets.QScrollArea(self)
        scroll_area3.setWidgetResizable(True)
        self.mbp = MotionPathBufferPaths()
        scroll_area3.setWidget(self.mbp)
        self.tabWidget.addTab(scroll_area3, "Buffered Paths")

        # Settings tab
        scroll_area2 = QtWidgets.QScrollArea(self)
        scroll_area2.setWidgetResizable(True)
        mws = MotionPathWidgetSettings()
        scroll_area2.setWidget(mws)
        self.tabWidget.addTab(scroll_area2, "Settings")

        # Connect signals
        self.mws.enabled.connect(self._enabled)
        self.tabWidget.currentChanged.connect(self._tab_changed)

    def set_active_tool( self, tool ):
        """Set active tool"""
        self.mws._edit_buttons.set_active_tool(tool)

    @Slot(bool)
    def _enabled( self, value ):
        """Handle enable state change"""
        if not value:
            self.mbp.empty_buffer_paths()
        self.mbp.setEnabled(value)

    def enable_tool( self ):
        """Enable tool if checkbox is checked"""
        if self.mws._enable_button.isChecked():
            _enable_motion_path(True)

    @Slot(int)
    def _tab_changed( self, value ):
        """Handle tab change"""
        if self.tabWidget.tabText(value) != "Buffered Paths":
            self.mbp.deselect_all()

    def empty_buffer_paths( self ):
        """Empty all buffer paths"""
        self.mbp.empty_buffer_paths()

    def closeEvent( self, event ):
        """Handle close event"""
        global _viewport_original_settings

        # Disable motion path functionality
        try:
            _enable_motion_path(False)
        except RuntimeError as e:
            # Widget already deleted - this is expected during Maya shutdown
            pass
        except Exception as e:
            cmds.warning(f"Error disabling motion path on close: {e}")

        # Clean up viewport overrides
        _cleanup_viewport_overrides()

        # Clear global reference to prevent stale instances
        global tc_motion_path_widget
        if tc_motion_path_widget is self:
            tc_motion_path_widget = None

        # Force refresh all viewports to clear any rendering artifacts
        try:
            cmds.refresh(force=True)
        except:
            pass

        # Call parent closeEvent
        super(MotionPathWidget, self).closeEvent(event)


#############################################################
# GLOBAL WIDGET INSTANCE
#############################################################

tc_motion_path_widget = None

# Store original viewport override settings for each VP2 panel
# Format: {panel_name: {'renderer': str, 'override': str}}
_viewport_original_settings = {}


def _get_dockable_motion_path_widget():
    """Get or create the motion path widget"""
    global tc_motion_path_widget

    # Check if existing instance is still valid
    if tc_motion_path_widget is not None:
        try:
            # Try to access a property to check if the C++ object is still valid
            _ = tc_motion_path_widget.objectName()

            # If valid and visible, close it
            if tc_motion_path_widget.isVisible():
                tc_motion_path_widget.close()
        except RuntimeError:
            # Object has been deleted at the C++ level - this is normal
            tc_motion_path_widget = None
        except AttributeError as e:
            # Object was partially deleted
            cmds.warning(f"Motion Path widget in invalid state: {e}")
            tc_motion_path_widget = None
        except Exception as e:
            cmds.warning(f"Unexpected error checking motion path widget: {e}")
            tc_motion_path_widget = None

    # Create new instance
    tc_motion_path_widget = MotionPathWidget()
    return tc_motion_path_widget


def _open_gui():
    """Open the motion path GUI"""
    # Check if plugin is loaded
    plugin_name = 'motionPath'
    if not cmds.pluginInfo(plugin_name, query=True, loaded=True):
        try:
            cmds.loadPlugin(plugin_name)
            print(f"Loaded {plugin_name} plugin")
        except RuntimeError as e:
            cmds.error(f"Failed to load {plugin_name} plugin: {e}. Please ensure the .mll file is in Maya's plugin path.")
            return
        except Exception as e:
            cmds.error(f"Unexpected error loading {plugin_name} plugin: {e}")
            traceback.print_exc()
            return

    # Close existing workspace control if it exists
    if cmds.workspaceControl(WCN, q=True, exists=True):
        try:
            # First, disable motion path if it was enabled
            _enable_motion_path(False)
        except RuntimeError as e:
            # Widget already deleted - safe to ignore
            pass
        except Exception as e:
            cmds.warning(f"Error disabling motion path: {e}")

        try:
            # Close the workspace control gracefully
            cmds.workspaceControl(WCN, e=True, close=True)
            # Wait one frame to ensure close completes
            cmds.refresh()
        except RuntimeError:
            # Workspace control already deleted
            pass
        except Exception as e:
            # Other errors can be safely ignored during cleanup
            pass

        try:
            # Delete the workspace control
            # Note: For workspace controls, use deleteUI without control=True flag
            # The control=True flag is for legacy UI elements and causes errors
            # when the workspace control is in floating state
            cmds.deleteUI(WCN)
        except RuntimeError:
            # UI already deleted
            pass
        except Exception as e:
            # Safe to ignore during cleanup
            pass

    # Create and show widget
    try:
        widget = _get_dockable_motion_path_widget()
        # Use retain=False to avoid issues with workspace control state persistence
        widget.show(dockable=True, area='right', floating=False, retain=False)
    except Exception as e:
        cmds.error(f"Failed to create Motion Path widget: {e}")
        traceback.print_exc()
        return

    # Configure workspace control
    try:
        cmds.workspaceControl(WCN, e=True,
                              ttc=["AttributeEditor", -1],
                              wp="preferred", mw=420)
    except Exception as e:
        cmds.warning(f"Error configuring workspace control: {e}")

    try:
        widget.raise_()
    except RuntimeError:
        # Widget was deleted immediately after creation - unusual but handle it
        cmds.warning("Motion Path widget was unexpectedly deleted")

    # Do NOT call enable_tool() here - let the user click the enable button
    # This prevents the confusing state where GUI is open but motion path is disabled


#############################################################
# PUBLIC API
#############################################################

def run():
    """Run the motion path tool"""
    # Load plugin if not already loaded
    # if not cmds.pluginInfo('tcMotionPath', q=True, l=True):
    #     try:
    #         cmds.loadPlugin('tcMotionPath', quiet=True)
    #     except Exception as e:
    #         cmds.warning(f"Failed to load tcMotionPath plugin: {e}")
    #         return

    # Open GUI
    _open_gui()

    # Setup playback range and selection
    _playback_range()
    _refresh_selection()

    # Auto-enable motion path if there's a selection
    # This provides better UX - user sees paths immediately
    # Simulate user clicking the enable button to ensure consistent behavior
    global tc_motion_path_widget
    if tc_motion_path_widget and cmds.ls(sl=True):
        try:
            # Verify widget and its components are still valid
            if not tc_motion_path_widget.mws or not tc_motion_path_widget.mws._enable_button:
                raise AttributeError("Motion Path UI components not properly initialized")

            # Simulate user click by setting checked state and calling toggle method
            tc_motion_path_widget.mws._enable_button.setChecked(True)
            tc_motion_path_widget.mws._toggle_enable()
            print("Motion Path enabled with current selection")
        except AttributeError as e:
            cmds.warning(f"Motion Path UI not fully initialized: {e}")
        except RuntimeError as e:
            cmds.warning(f"Motion Path UI object was deleted: {e}")
        except Exception as e:
            cmds.error(f"Failed to auto-enable Motion Path: {e}")
            traceback.print_exc()

# End of file