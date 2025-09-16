#include <stdio.h>
#include <iostream>
#include <fstream>
#include <string.h>
#include <stdlib.h>
#include <string>
#include <vector>
#include <GL/glew.h>
#include <GLFW/glfw3.h>

#include "imgui.h"
#include "backends/imgui_impl_glfw.h"
#include "backends/imgui_impl_opengl3.h"
#include "file_utils.h"
#include "math_utils.h"
#define GL_SILENCE_DEPRECATION

using namespace std;

/*   Variables */
char theProgramTitle[] = "MarbleSolitaire";
int theWindowWidth = 800, theWindowHeight = 600;
int theWindowPositionX = 40, theWindowPositionY = 40;

// OpenGL variables - Vertex buffers for rendering
GLuint squareVAO, squareVBO, squareEBO;
GLuint circleVAO, circleVBO;
GLuint highlightVAO, highlightVBO, highlightEBO;
GLuint gmodelLocation;
GLuint gProjectionLocation;
GLuint gColorLocation;

/* Constants */
const int ANIMATION_DELAY = 20; /* milliseconds between rendering */
const char *pVSFileName = "shaders/shader.vs";
const char *pFSFileName = "shaders/shader.fs";
const int CIRCLE_SEGMENTS = 32;

// Game board configuration constants
const int BOARD_SIZE = 7;		   
const float SQUARE_SIZE = 0.1f;	   
const float MARBLE_RADIUS = 0.04f; 

enum MarbleState
{
	EMPTY,
	MARBLE
};

enum GameStatus
{
	PLAYING,
	WON,
	LOST
};

enum MoveError {
    NONE,
    NO_MARBLE_SELECTED,
    DESTINATION_NOT_EMPTY,
    INVALID_DISTANCE,
    NO_MARBLE_TO_JUMP
};


// constants to indicate error
MoveError lastMoveError = NONE;
bool showMoveError = false;
double moveErrorTime = 0.0;
const double ERROR_DISPLAY_TIME = 2.0; // seconds

// position on the board
struct Position
{
	int row;
	int col;
};

struct Move
{
	Position from;
	Position to;
	Position captured; // Position of the captured marble
};

// Game state
MarbleState board[BOARD_SIZE][BOARD_SIZE]; // Game board
GameStatus gameStatus = PLAYING;
Position selectedPosition = {-1, -1}; // No selection initially
vector<Move> moveHistory;
int currentMoveIndex = -1; // For undo/redo functionality
float gameTime = 0.0f;
int remainingMarbles = 0;
bool isDragging = false;

/********************************************************************
  Utility functions
 */

void initializeBoard()
{
	//reset remaining marbles counter
	remainingMarbles = 0;

	for (int i = 0; i < BOARD_SIZE; i++)
	{
		for (int j = 0; j < BOARD_SIZE; j++)
		{
			board[i][j] = EMPTY;
		}
	}

	for (int i = 0; i < BOARD_SIZE; i++)
	{
		for (int j = 0; j < BOARD_SIZE; j++)
		{
			bool isValid = true;

			if ((i < 2 || i > 4) && (j < 2 || j > 4))
			{
				isValid = false;
			}

			if (isValid)
			{
				board[i][j] = MARBLE;
				remainingMarbles++;
			}
		}
	}

	// Center position is empty
	board[BOARD_SIZE / 2][BOARD_SIZE / 2] = EMPTY;
	remainingMarbles--;

	moveHistory.clear();
	currentMoveIndex = -1;
	gameTime = 0.0f;
	gameStatus = PLAYING;
}

static void createSquareBuffer()
{
	Vector3f squareVertices[] = {
		Vector3f(-0.5f, -0.5f, 0.0f),
		Vector3f(0.5f, -0.5f, 0.0f),
		Vector3f(0.5f, 0.5f, 0.0f),
		Vector3f(-0.5f, 0.5f, 0.0f)};

	GLuint indices[] = {
		0, 1, 2, 
		0, 2, 3	 
	};

	glGenVertexArrays(1, &squareVAO);
	glBindVertexArray(squareVAO);

	glGenBuffers(1, &squareVBO);
	glBindBuffer(GL_ARRAY_BUFFER, squareVBO);
	glBufferData(GL_ARRAY_BUFFER, sizeof(squareVertices), squareVertices, GL_STATIC_DRAW);

	glGenBuffers(1, &squareEBO);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, squareEBO);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);

	glEnableVertexAttribArray(0);
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vector3f), (void *)0);

	glBindVertexArray(0);
	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
	cout << "Square buffer created\n";
}

static void createCircleBuffer()
{
	std::vector<Vector3f> vertices;
	std::vector<GLuint> indices;

	// Center vertex
	vertices.push_back(Vector3f(0.0f, 0.0f, 0.1f));

	// Outer vertices
	for (int i = 0; i <= CIRCLE_SEGMENTS; i++)
	{
		float angle = 2.0f * M_PI * i / CIRCLE_SEGMENTS;
		vertices.push_back(Vector3f(cos(angle), sin(angle), 0.1f));
	}

	// Create indices for triangle fan
	for (int i = 1; i <= CIRCLE_SEGMENTS; i++)
	{
		indices.push_back(0); // Center
		indices.push_back(i);
		indices.push_back(i % CIRCLE_SEGMENTS + 1);
	}

	glGenVertexArrays(1, &circleVAO);
	glBindVertexArray(circleVAO);

	glGenBuffers(1, &circleVBO);
	glBindBuffer(GL_ARRAY_BUFFER, circleVBO);
	glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(Vector3f), &vertices[0], GL_STATIC_DRAW);

	GLuint circleEBO;
	glGenBuffers(1, &circleEBO);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, circleEBO);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(GLuint), &indices[0], GL_STATIC_DRAW);

	glEnableVertexAttribArray(0);
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vector3f), (void *)0);

	glBindVertexArray(0);
}

static void createHighlightBuffer()
{
	Vector3f highlightVertices[] = {
		Vector3f(-0.55f, -0.55f, 0.0f),
		Vector3f(0.55f, -0.55f, 0.0f),
		Vector3f(0.55f, 0.55f, 0.0f),
		Vector3f(-0.55f, 0.55f, 0.0f)};

	GLuint indices[] = {
		0, 1, 2,
		0, 2, 3};

	glGenVertexArrays(1, &highlightVAO);
	glBindVertexArray(highlightVAO);

	glGenBuffers(1, &highlightVBO);
	glBindBuffer(GL_ARRAY_BUFFER, highlightVBO);
	glBufferData(GL_ARRAY_BUFFER, sizeof(highlightVertices), highlightVertices, GL_STATIC_DRAW);

	glGenBuffers(1, &highlightEBO);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, highlightEBO);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);

	glEnableVertexAttribArray(0);
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vector3f), (void *)0);

	glBindVertexArray(0);
	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
	cout << "Highlight buffer created\n";
}

static void AddShader(GLuint ShaderProgram, const char *pShaderText, GLenum ShaderType)
{
	GLuint ShaderObj = glCreateShader(ShaderType);

	if (ShaderObj == 0)
	{
		fprintf(stderr, "Error creating shader type %d\n", ShaderType);
		exit(0);
	}

	const GLchar *p[1];
	p[0] = pShaderText;
	GLint Lengths[1];
	Lengths[0] = strlen(pShaderText);
	glShaderSource(ShaderObj, 1, p, Lengths);
	glCompileShader(ShaderObj);
	GLint success;
	glGetShaderiv(ShaderObj, GL_COMPILE_STATUS, &success);
	if (!success)
	{
		GLchar InfoLog[1024];
		glGetShaderInfoLog(ShaderObj, 1024, NULL, InfoLog);
		fprintf(stderr, "Error compiling shader type %d: '%s'\n", ShaderType, InfoLog);
		exit(1);
	}

	glAttachShader(ShaderProgram, ShaderObj);
}

static void CompileShaders()
{
	GLuint ShaderProgram = glCreateProgram();

	if (ShaderProgram == 0)
	{
		fprintf(stderr, "Error creating shader program\n");
		exit(1);
	}

	string vs, fs;

	if (!ReadFile(pVSFileName, vs))
	{
		exit(1);
	}

	if (!ReadFile(pFSFileName, fs))
	{
		exit(1);
	}

	AddShader(ShaderProgram, vs.c_str(), GL_VERTEX_SHADER);
	AddShader(ShaderProgram, fs.c_str(), GL_FRAGMENT_SHADER);

	GLint Success = 0;
	GLchar ErrorLog[1024] = {0};

	glLinkProgram(ShaderProgram);
	glGetProgramiv(ShaderProgram, GL_LINK_STATUS, &Success);
	if (Success == 0)
	{
		glGetProgramInfoLog(ShaderProgram, sizeof(ErrorLog), NULL, ErrorLog);
		fprintf(stderr, "Error linking shader program: '%s'\n", ErrorLog);
		exit(1);
	}

	glValidateProgram(ShaderProgram);
	glGetProgramiv(ShaderProgram, GL_VALIDATE_STATUS, &Success);
	if (!Success)
	{
		glGetProgramInfoLog(ShaderProgram, sizeof(ErrorLog), NULL, ErrorLog);
		fprintf(stderr, "Invalid shader program1: '%s'\n", ErrorLog);
		exit(1);
	}

	glUseProgram(ShaderProgram);

	gmodelLocation = glGetUniformLocation(ShaderProgram, "model"); 
	gProjectionLocation = glGetUniformLocation(ShaderProgram, "projection");
	gColorLocation = glGetUniformLocation(ShaderProgram, "color");

	if (gmodelLocation == static_cast<GLuint>(-1))
		fprintf(stderr, "Warning: Couldn't find uniform 'model'\n");
	if (gProjectionLocation == static_cast<GLuint>(-1))
		fprintf(stderr, "Warning: Couldn't find uniform 'projection'\n");
	if (gColorLocation == static_cast<GLuint>(-1))
		fprintf(stderr, "Warning: Couldn't find uniform 'color'\n");
}

/***************game logic functions******************/

Position screenToBoard(double xpos, double ypos)
{
	float x = (2.0f * xpos) / theWindowWidth - 1.0f;
	float y = 1.0f - (2.0f * ypos) / theWindowHeight;

	float aspectRatio = (float)theWindowWidth / (float)theWindowHeight;
	float orthoSize = 1.0f;
	x *= orthoSize * aspectRatio;
	y *= orthoSize;

	float spacing = SQUARE_SIZE * 2.2f;
	int col = static_cast<int>(round((x / spacing) + BOARD_SIZE / 2));
	int row = static_cast<int>(round((BOARD_SIZE / 2) - (y / spacing)));

	if (row >= 0 && row < BOARD_SIZE && col >= 0 && col < BOARD_SIZE)
	{
		if (!((row < 2 || row > 4) && (col < 2 || col > 4)))
		{
			return {row, col};
		}
	}

	return {-1, -1}; 
}

bool isValidMove(Position from, Position to)
{
    if (board[from.row][from.col] != MARBLE) {
        lastMoveError = NO_MARBLE_SELECTED;
        return false;
    }

    if (board[to.row][to.col] != EMPTY) {
        lastMoveError = DESTINATION_NOT_EMPTY;
        return false;
    }

    int rowDiff = abs(to.row - from.row);
    int colDiff = abs(to.col - from.col);

    if ((rowDiff == 2 && colDiff == 0) || (rowDiff == 0 && colDiff == 2)) {
        int middleRow = (from.row + to.row) / 2;
        int middleCol = (from.col + to.col) / 2;

        if (board[middleRow][middleCol] == MARBLE) {
            lastMoveError = NONE;
            return true;
        } else {
            lastMoveError = NO_MARBLE_TO_JUMP;
            return false;
        }
    }
    
    lastMoveError = INVALID_DISTANCE;
    return false;
}

bool hasAvailableMoves()
{
	for (int i = 0; i < BOARD_SIZE; i++)
	{
		for (int j = 0; j < BOARD_SIZE; j++)
		{
			if (board[i][j] == MARBLE)
			{
				Position directions[4] = {{-2, 0}, {2, 0}, {0, -2}, {0, 2}};

				for (auto &dir : directions)
				{
					int newRow = i + dir.row;
					int newCol = j + dir.col;

					if (newRow >= 0 && newRow < BOARD_SIZE &&
						newCol >= 0 && newCol < BOARD_SIZE)
					{
						if (isValidMove({i, j}, {newRow, newCol}))
						{
							return true;
						}
					}
				}
			}
		}
	}

	return false;
}

void checkGameStatus()
{
	if (remainingMarbles == 1)
	{
		gameStatus = WON;
		return;
	}

	if (!hasAvailableMoves() && remainingMarbles > 1)
	{
		gameStatus = LOST;
	}
}

// Make a move
void makeMove(Position from, Position to)
{
	if (!isValidMove(from, to)) {
        showMoveError = true;
        moveErrorTime = glfwGetTime(); 
        return;
    }

	int middleRow = (from.row + to.row) / 2;
	int middleCol = (from.col + to.col) / 2;

	board[from.row][from.col] = EMPTY;
	board[to.row][to.col] = MARBLE;
	board[middleRow][middleCol] = EMPTY;

	remainingMarbles--;

	Move move = {from, to, {middleRow, middleCol}};

	if (currentMoveIndex < static_cast<int>(moveHistory.size()) - 1)
	{
		moveHistory.resize(currentMoveIndex + 1);
	}

	moveHistory.push_back(move);
	currentMoveIndex = moveHistory.size() - 1;

	checkGameStatus();
}

void undoMove()
{
	if (currentMoveIndex < 0)
		return;

	Move &move = moveHistory[currentMoveIndex];

	board[move.from.row][move.from.col] = MARBLE;
	board[move.to.row][move.to.col] = EMPTY;
	board[move.captured.row][move.captured.col] = MARBLE;

	remainingMarbles++;
	currentMoveIndex--;
	gameStatus = PLAYING; 
}

void redoMove()
{
	if (currentMoveIndex >= static_cast<int>(moveHistory.size()) - 1)
		return; 

	currentMoveIndex++;
	Move &move = moveHistory[currentMoveIndex];

	board[move.from.row][move.from.col] = EMPTY;
	board[move.to.row][move.to.col] = MARBLE;
	board[move.captured.row][move.captured.col] = EMPTY;

	remainingMarbles--;

	checkGameStatus();
}

/********************************************************************
 Callback Functions
 */

void onInit(int argc, char *argv[])
{
	glClearColor(0.0f, 0.0f, 0.0f, 1.0f);

	createSquareBuffer();
	createCircleBuffer();
	createHighlightBuffer();

	CompileShaders();

	// glEnable(GL_DEPTH_TEST);
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
}

static void renderBoard()
{
	Matrix4f projection;
	float aspectRatio = (float)theWindowWidth / (float)theWindowHeight;
	float orthoSize = 1.0f;
	projection.InitIdentity();
	projection.m[0][0] = 1.0f / (orthoSize * aspectRatio);
	projection.m[1][1] = 1.0f / orthoSize;
	projection.m[2][2] = -1.0f;
	projection.m[3][3] = 1.0f;

	glUniformMatrix4fv(gProjectionLocation, 1, GL_FALSE, &projection.m[0][0]);

	glUniform3f(gColorLocation, 1.0f, 0.3f, 0.3f);

	Matrix4f backgroundModel;
	backgroundModel.InitIdentity();
	backgroundModel.m[0][0] = 0.8f;
	backgroundModel.m[1][1] = 0.8f;
	backgroundModel.m[2][2] = 1.1f;

	glUniformMatrix4fv(gmodelLocation, 1, GL_FALSE, &backgroundModel.m[0][0]);

	for (int i = 0; i < BOARD_SIZE; i++)
	{
		for (int j = 0; j < BOARD_SIZE; j++)
		{
			if ((i < 2 || i > 4) && (j < 2 || j > 4))
				continue;

			float x = (j - BOARD_SIZE / 2) * SQUARE_SIZE * 2.2f;
			float y = (BOARD_SIZE / 2 - i) * SQUARE_SIZE * 2.2f;

			if (selectedPosition.row == i && selectedPosition.col == j)
			{
				glBindVertexArray(highlightVAO);
				glUniform3f(gColorLocation, 1.0f, 1.0f, 0.0f);

				Matrix4f highlightModel;
				highlightModel.InitIdentity();
				highlightModel.m[3][0] = x;
				highlightModel.m[3][1] = y;
				highlightModel.m[0][0] = SQUARE_SIZE;
				highlightModel.m[1][1] = SQUARE_SIZE;

				glUniformMatrix4fv(gmodelLocation, 1, GL_FALSE, &highlightModel.m[0][0]);
				glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
				glBindVertexArray(0);
			}

			glBindVertexArray(squareVAO);
			glUniform3f(gColorLocation, 0.5f, 0.5f, 0.5f);

			Matrix4f squareModel;
			squareModel.InitIdentity();
			squareModel.m[3][0] = x;
			squareModel.m[3][1] = y;
			squareModel.m[0][0] = SQUARE_SIZE;
			squareModel.m[1][1] = SQUARE_SIZE;

			glUniformMatrix4fv(gmodelLocation, 1, GL_FALSE, &squareModel.m[0][0]);
			glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
			glBindVertexArray(0);

			if (board[i][j] == MARBLE)
			{
				// cout << "Drawing marble at position (" << i << "," << j << ")" << endl;
				glBindVertexArray(circleVAO);
				glUniform3f(gColorLocation, 0.8f, 0.2f, 0.2f);

				Matrix4f marbleModel;
				marbleModel.InitIdentity();
				marbleModel.m[3][0] = x;
				marbleModel.m[3][1] = y;
				marbleModel.m[0][0] = MARBLE_RADIUS;
				marbleModel.m[1][1] = MARBLE_RADIUS;

				glUniformMatrix4fv(gmodelLocation, 1, GL_FALSE, &marbleModel.m[0][0]);
				glDrawElements(GL_TRIANGLES, CIRCLE_SEGMENTS * 3, GL_UNSIGNED_INT, 0);
				glUniform3f(gColorLocation, 1.0f, 1.0f, 1.0f); // White center
				Matrix4f dotModel = marbleModel;
				dotModel.m[0][0] = MARBLE_RADIUS * 0.1f;
				dotModel.m[1][1] = MARBLE_RADIUS * 0.1f;
				glUniformMatrix4fv(gmodelLocation, 1, GL_FALSE, &dotModel.m[0][0]);
				glDrawArrays(GL_TRIANGLE_FAN, 0, CIRCLE_SEGMENTS + 2);

				glBindVertexArray(0);
			}
		}
	}
}

void mouse_button_callback(GLFWwindow *window, int button, int action, int mods)
{
	if (gameStatus != PLAYING)
		return; 

	double xpos, ypos;
	glfwGetCursorPos(window, &xpos, &ypos);
	Position boardPos = screenToBoard(xpos, ypos);

	if (button == GLFW_MOUSE_BUTTON_LEFT)
	{
		if (action == GLFW_PRESS)
		{
			if (boardPos.row >= 0 && board[boardPos.row][boardPos.col] == MARBLE)
			{
				selectedPosition = boardPos;
				isDragging = true;
			}
		}
		else if (action == GLFW_RELEASE && isDragging)
		{
			isDragging = false;

			if (selectedPosition.row >= 0 && boardPos.row >= 0)
			{
				makeMove(selectedPosition, boardPos);
			}

			selectedPosition = {-1, -1}; 
		}
	}
}

void mouse_callback(GLFWwindow *window, double xpos, double ypos)
{
	if (isDragging)
	{
		Position boardPos = screenToBoard(xpos, ypos);
		if (boardPos.row >= 0 && board[boardPos.row][boardPos.col] == EMPTY)
		{
			// Potentially highlight valid drop targets
		}
	}
}

void key_callback(GLFWwindow *window, int key, int scancode, int action, int mods)
{
	if (action == GLFW_PRESS)
	{
		switch (key)
		{
		case GLFW_KEY_R:
			initializeBoard();
			break;

		case GLFW_KEY_Z:
			if (mods & GLFW_MOD_CONTROL && currentMoveIndex >= 0)
			{
				undoMove();
			}
			break;

		case GLFW_KEY_Y:
			if (mods & GLFW_MOD_CONTROL && currentMoveIndex < static_cast<int>(moveHistory.size()) - 1)
			{
				redoMove();
			}
			break;

		case GLFW_KEY_ESCAPE:
			glfwSetWindowShouldClose(window, true);
			break;
		}
	}
}

void InitImGui(GLFWwindow *window)
{
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGuiIO &io = ImGui::GetIO();
	(void)io;
	ImGui::StyleColorsDark();
	ImGui_ImplGlfw_InitForOpenGL(window, true);
	ImGui_ImplOpenGL3_Init("#version 330");
}

void RenderImGui()
{
	ImGui_ImplOpenGL3_NewFrame();
	ImGui_ImplGlfw_NewFrame();
	ImGui::NewFrame();

	// Create a game status window
	ImGui::SetNextWindowPos(ImVec2(10, 10));
	ImGui::SetNextWindowSize(ImVec2(200, 100));
	ImGui::Begin("Game Status", nullptr, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove);

	ImGui::Text("Time: %.1f seconds", gameTime);
	ImGui::Text("Marbles Remaining: %d", remainingMarbles);

	if (gameStatus == WON)
	{
		ImGui::TextColored(ImVec4(0, 1, 0, 1), "You Won!");
	}
	else if (gameStatus == LOST)
	{
		ImGui::TextColored(ImVec4(1, 0, 0, 1), "Game Over!");
	}

	ImGui::End();

	// Create a control panel window
	ImGui::SetNextWindowPos(ImVec2(theWindowWidth - 230, 10));
	ImGui::SetNextWindowSize(ImVec2(200, 130));
	ImGui::Begin("Controls", nullptr, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove);

	if (ImGui::Button("Reset Game", ImVec2(180, 30)))
	{
		initializeBoard();
	}

	if (ImGui::Button("Undo", ImVec2(85, 30)))
	{
		if (currentMoveIndex >= 0)
		{
			undoMove();
		}
	}

	ImGui::SameLine();

	if (ImGui::Button("Redo", ImVec2(85, 30)))
	{
		if (currentMoveIndex < static_cast<int>(moveHistory.size()) - 1)
		{
			redoMove();
		}
	}

	ImGui::End();

	if (showMoveError) {
        double currentTime = glfwGetTime();
        if (currentTime - moveErrorTime > ERROR_DISPLAY_TIME) {
            showMoveError = false;
        } else {
            ImGui::SetNextWindowPos(ImVec2(theWindowWidth/2 - 150, theWindowHeight - 80));
            ImGui::SetNextWindowSize(ImVec2(300, 60));
            ImGui::Begin("Error", nullptr, ImGuiWindowFlags_NoTitleBar | 
                                         ImGuiWindowFlags_NoResize | 
                                         ImGuiWindowFlags_NoMove |
                                         ImGuiWindowFlags_NoCollapse);
            
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.3f, 0.3f, 1.0f));
            switch (lastMoveError) {
                case NO_MARBLE_SELECTED:
                    ImGui::Text("You must select a marble to move!");
                    break;
                case DESTINATION_NOT_EMPTY:
                    ImGui::Text("You can only move to empty positions!");
                    break;
                case INVALID_DISTANCE:
                    ImGui::Text("Marbles must jump exactly 2 spaces!");
                    break;
                case NO_MARBLE_TO_JUMP:
                    ImGui::Text("You must jump over another marble!");
                    break;
                default:
                    ImGui::Text("Invalid move!");
                    break;
            }
            ImGui::PopStyleColor();
            
            ImGui::End();
        }
    }

	ImGui::Render();																
	ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}

int main(int argc, char *argv[])
{
	if (!glfwInit())
	{
		fprintf(stderr, "Failed to initialize GLFW\n");
		return -1;
	}

	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 2);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
	glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
	glfwWindowHint(GLFW_RESIZABLE, GL_FALSE);

	GLFWwindow *window = glfwCreateWindow(800, 600, "Marble Solitaire", NULL, NULL);
	glfwMakeContextCurrent(window);

	if (!window)
	{
		glfwTerminate();
		return 0;
	}

	glewExperimental = GL_TRUE;
	glewInit();
	printf("GL version: %s\n",
		   glGetString(GL_VERSION));
	onInit(argc, argv);

	initializeBoard();

	glfwSetKeyCallback(window, key_callback);
	glfwSetMouseButtonCallback(window, mouse_button_callback);
	glfwSetCursorPosCallback(window, mouse_callback);

	InitImGui(window);

	double lastFrameTime = glfwGetTime();

	while (!glfwWindowShouldClose(window))
	{
		double currentTime = glfwGetTime();
		double deltaTime = currentTime - lastFrameTime;
		lastFrameTime = currentTime;

		if (gameStatus == PLAYING)
		{
			gameTime += deltaTime;
		}

		glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		renderBoard();

		RenderImGui();

		glfwSwapBuffers(window);
		glfwPollEvents();
	}

	glDeleteVertexArrays(1, &squareVAO);
	glDeleteBuffers(1, &squareVBO);
	glDeleteVertexArrays(1, &circleVAO);
	glDeleteBuffers(1, &circleVBO);
	glDeleteVertexArrays(1, &highlightVAO);
	glDeleteBuffers(1, &highlightVBO);

	ImGui_ImplOpenGL3_Shutdown();
	ImGui_ImplGlfw_Shutdown();
	ImGui::DestroyContext();

	glfwTerminate();
	return 0;
}
