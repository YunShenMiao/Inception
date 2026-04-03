# ================================= COLORS =================================== #
RED			= \033[0;31m
GREEN		= \033[0;32m
CYAN		= \033[0;36m
RESET		= \033[0m
# ================================= PROGRAM ================================== #
NAME		= webserv
# ================================ COMPILER ================================== #
CXX			= c++
CXXFLAGS	= -Wall -Wextra -Werror -std=c++17
INCLUDES	= -I./include

ifdef DEBUG
CXXFLAGS += -DDEBUG
endif
# ================================= SOURCES ================================== #
SRC_DIR		= src
OBJ_DIR		= obj

PARSER_SRCS			=	configParser.cpp \
			  			grammarValidation.cpp \
			  			parseGlobal.cpp \
			  			parseServer.cpp \
			  			parseLocation.cpp \
			  			print.cpp 
HTTP_REQUEST_SRCS	= 	httpRequest.cpp
HTTP_RESPONSE_SRCS	=	httpResponse.cpp \
						requesthandler.cpp

SERVER_SRCS 		=	server.cpp
UTILS_SRCS			=	utils.cpp
POLLER_SRCS			=	poller.cpp
ROUTER_SRCS			=	router.cpp
CGI_SRCS			=	cgiManager.cpp
BOT_DETECTION_SRCS	=	botDetection.cpp
CAPTCHA_BYPASS_SRCS	=	captchaBypass.cpp
MAIN_SRC			=	main.cpp

SRCS 		= $(addprefix $(SRC_DIR)/poller/, $(POLLER_SRCS)) $(addprefix $(SRC_DIR)/configParser/, $(PARSER_SRCS)) $(addprefix $(SRC_DIR)/utils/, $(UTILS_SRCS)) $(addprefix $(SRC_DIR)/, $(MAIN_SRC)) $(addprefix $(SRC_DIR)/server/, $(SERVER_SRCS)) $(addprefix $(SRC_DIR)/httpRequest/, $(HTTP_REQUEST_SRCS)) $(addprefix $(SRC_DIR)/router/, $(ROUTER_SRCS)) $(addprefix $(SRC_DIR)/httpResponse/, $(HTTP_RESPONSE_SRCS)) $(addprefix $(SRC_DIR)/cgi/, $(CGI_SRCS)) $(addprefix $(SRC_DIR)/botDetection/, $(BOT_DETECTION_SRCS)) $(addprefix $(SRC_DIR)/captchaBypass/, $(CAPTCHA_BYPASS_SRCS))
OBJS		= $(patsubst $(SRC_DIR)/%.cpp,$(OBJ_DIR)/%.o,$(SRCS))
DEPS		= $(OBJS:.o=.d)
# ================================== RULES =================================== #
.PHONY: all clean fclean re

all: $(NAME)

$(NAME): $(OBJS)
	@$(CXX) $(CXXFLAGS) $(OBJS) -o $(NAME)
	@echo "$(GREEN) $(NAME) compiled successfully$(RESET)"

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.cpp
	@mkdir -p $(dir $@)
	@echo "$(CYAN) Compiling $<$(RESET)"
	@$(CXX) $(CXXFLAGS) $(INCLUDES) -MMD -MP -c $< -o $@

-include $(DEPS)

clean:
	@rm -rf $(OBJ_DIR)
	@echo "$(GREEN) Clean complete!$(RESET)"

fclean: clean
	@rm -f $(NAME)
	@echo "$(GREEN) Full clean complete!$(RESET)"

re: fclean all
