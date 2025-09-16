# Marble Solitaire

## Overview
This project implements a classic Marble Solitaire game using modern OpenGL for rendering and Dear ImGui for user interface elements. The game follows traditional English peg solitaire rules where players jump marbles over each other to remove them from the board, with the goal of leaving only one marble.

## Instructions to Run the Code

### Prerequisites
- OpenGL 3.2+
- GLEW
- GLFW3
- ImGui
- A C++ compiler (g++ recommended)

### Build Instructions
1. Make sure all dependencies are installed on your system
2. Clone the repository
3. Build the project using the provided Makefile, so run the following command:
   ```
   make
   ```
4. Run the executable:
   ```
   ./marble_solitaire
   ```

## Game Rules
1. The game starts with marbles arranged in a cross pattern, with the center position empty.
2. Click on a marble to select it, then click on a valid destination (two positions away, with a marble in between).
3. If the move is valid, the selected marble jumps over the adjacent marble, removing it from the board.
4. The goal is to remove as many marbles as possible, ideally leaving only one marble on the board.

## Controls
- **Mouse Left Click**: Select and move marbles
- **R key**: Reset the game
- **Ctrl+Z**: Undo move
- **Ctrl+Y**: Redo move
- **ESC key**: Exit the game

## Implementation Details

### Graphics
- Modern OpenGL with vertex and fragment shaders
- All rendering is done on the GPU using shaders
- Three primitive types: squares (for board cells), circles (for marbles), and highlight overlays

### Game Logic
- Board representation using a 2D grid
- Move validation and execution
- Win/loss condition checking
- Move history tracking for undo/redo functionality

### ImGui Integration
ImGui is incorporated to provide a clean user interface with:
1. A game status panel showing:
   - Time elapsed
   - Number of marbles remaining
   - Game outcome messages (win/lose)
2. A control panel with buttons for:
   - Resetting the game
   - Undoing moves
   - Redoing moves
3. Error validation feedback:
   - Visual notifications for invalid moves
   - Clear explanations of rule violations
   - Timed display of error messages

The ImGui interface is placed at the edges of the screen to maintain focus on the game board. The integration provides direct visual feedback about the game state and allows for intuitive control of game functions beyond keyboard shortcuts.

## Implementation Observations

### Effort Distribution
1. **Setting up OpenGL rendering pipeline**:
   - Creating VAOs/VBOs for different primitive types
   - Implementing shaders
   - Managing transformations and rendering

2. **Game Logic Implementation**: 
   - Board state representation
   - Move validation
   - Win/loss condition checking
   - History tracking for undo/redo

3. **User Interaction**:
   - Converting screen coordinates to board positions
   - Handling mouse input for selection and movement
   - Implementing keyboard shortcuts

4. **ImGui Integration**:
   - Setting up ImGui with GLFW/OpenGL
   - Creating status displays and controls
   - Styling and positioning UI elements

### Challenges
1. **Coordinate system matching**: Ensuring consistent conversion between screen coordinates and board positions required careful attention.
2. **Marble rendering**: Getting circles to render properly with the shader pipeline required more effort than anticipated.
3. **State management**: Keeping track of the game state, selected marbles, and move history required careful design.

