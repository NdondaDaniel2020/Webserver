# **************************************************************************** #
#                                                                              #
#                                                         :::      ::::::::    #
#    Makefile                                           :+:      :+:    :+:    #
#                                                     +:+ +:+         +:+      #
#    By: nmatondo <marvin@42.fr>                    +#+  +:+       +#+         #
#                                                 +#+#+#+#+#+   +#+            #
#    Created: 2025/07/11 10:11:23 by nmatondo          #+#    #+#              #
#    Updated: 2026/04/07 08:25:28 by nmatondo         ###   ########.fr        #
#                                                                              #
# **************************************************************************** #

NAME		= webserv
CXX			= c++
CXXFLAGS	= -Wall -Wextra -Werror -std=c++98 -g
INCLUDE		= -I./include
OBJDIR		= obj

SRCDIR		= src
SOURCES		= $(SRCDIR)/main.cpp \
			  $(SRCDIR)/server/Server.cpp \
			  $(SRCDIR)/server/ServerConnections.cpp \
			  $(SRCDIR)/server/ServerEventLoop.cpp \
			  $(SRCDIR)/utils/FileUtils.cpp \
			  $(SRCDIR)/config/ConfigParser.cpp \
			  $(SRCDIR)/config/ConfigHelper.cpp \
			  $(SRCDIR)/config/ConfigValidator.cpp \
			  $(SRCDIR)/utils/StringUtils.cpp \
			  $(SRCDIR)/http/Response.cpp \
			  $(SRCDIR)/http/StatusCodes.cpp \
			  $(SRCDIR)/http/HttpRequest.cpp \
			  $(SRCDIR)/http/HttpMethods.cpp \
			  $(SRCDIR)/http/ResponseDirectoryListing.cpp \
			  $(SRCDIR)/http/ResponseMultipart.cpp \
			  $(SRCDIR)/http/ResponseHelpers.cpp \
			  $(SRCDIR)/server/Client.cpp \
			  $(SRCDIR)/server/ClientRequest.cpp \
			  $(SRCDIR)/server/ClientProcessing.cpp \
			  $(SRCDIR)/server/ClientCgi.cpp \
			  $(SRCDIR)/cgi/EnvBuilder.cpp \

OBJECTS		= $(SOURCES:$(SRCDIR)/%.cpp=$(OBJDIR)/%.o)

HEADERS		= include/Server.hpp \
			  include/FileUtils.hpp \
			  include/ConfigParser.hpp \
			  include/StringUtils.hpp \
			  include/Response.hpp \
			  include/HttpRequest.hpp \
			  include/Client.hpp \
			  include/ConfigValidator.hpp \
			  include/EnvBuilder.hpp \



all: $(NAME)

$(NAME): $(OBJECTS)
	@echo "🔗 Linking $(NAME)..."
	@$(CXX) $(CXXFLAGS) $(OBJECTS) -o $(NAME)
	@echo "✅ $(NAME) compiled successfully!"

$(OBJDIR)/%.o: $(SRCDIR)/%.cpp $(HEADERS)
	@mkdir -p $(dir $@)
	@echo "🔨 Compiling $<..."
	@$(CXX) $(CXXFLAGS) $(INCLUDE) -c $< -o $@

clean:
	@echo "🧹 Cleaning object files..."
	@rm -rf $(OBJDIR)

fclean: clean
	@echo "🗑️  Removing $(NAME)..."
	@rm -rf $(NAME)

re: fclean all


run: $(NAME)
	@echo "🚀 Running $(NAME)..."
	@./$(NAME) config/default.conf

valgrind: $(NAME)
	@echo "🔍 Running $(NAME) with Valgrind..."
	@valgrind --leak-check=full --show-leak-kinds=all --track-origins=yes --verbose ./$(NAME) config/default.conf

test: $(NAME)
	@echo "🧪 Running tests..."
	@if [ -f tests/test_requests.http ]; then \
		echo "📝 HTTP test file found"; \
	fi
	@if [ -f tests/test_upload.py ]; then \
		echo "🐍 Running upload test..."; \
		python3 tests/test_upload.py; \
	fi
	@if [ -f tests/stress_test.py ]; then \
		echo "💪 Running stress test..."; \
		python3 tests/stress_test.py; \
	fi

debug: CXXFLAGS += -DDEBUG -fsanitize=address -fsanitize=undefined
debug: $(NAME)

test-suite: $(NAME)
	@echo "🧪 Running comprehensive test suite..."
	@echo "Starting server on http://127.0.0.1:8080..."
	@./$(NAME) conf/test.conf &
	@SERVER_PID=$$!; \
	sleep 2; \
	if ./test/test_webserver.sh http://127.0.0.1:8080; then \
		echo "✅ Test suite completed successfully!"; \
		kill $$SERVER_PID 2>/dev/null || true; \
		exit 0; \
	else \
		echo "❌ Test suite failed!"; \
		kill $$SERVER_PID 2>/dev/null || true; \
		exit 1; \
	fi

test-stress: $(NAME)
	@echo "💪 Running stress test with Siege..."
	@if ! command -v siege &> /dev/null; then \
		echo "❌ Siege not found. Install with: brew install siege"; \
		exit 1; \
	fi
	@echo "Starting server on http://127.0.0.1:8080..."
	@./$(NAME) conf/test.conf &
	@SERVER_PID=$$!; \
	sleep 2; \
	echo "Running 50 concurrent users for 10 seconds..."; \
	siege -c50 -t10S -b http://127.0.0.1:8080/; \
	kill $$SERVER_PID 2>/dev/null || true

test-memory: $(NAME)
	@echo "🔍 Running memory leak test with Valgrind..."
	@echo "Starting server on http://127.0.0.1:8080..."
	@valgrind --leak-check=full --track-fds=yes ./$(NAME) conf/test.conf &
	@SERVER_PID=$$!; \
	sleep 3; \
	./test/test_webserver.sh http://127.0.0.1:8080; \
	echo "Stopping server..."; \
	kill $$SERVER_PID 2>/dev/null || true; \
	sleep 1

help:
	@echo "📖 Available targets:"
	@echo "  all         - Build the project"
	@echo "  clean       - Remove object files"
	@echo "  fclean      - Remove object files and executable"
	@echo "  re          - Rebuild the project"
	@echo "  run         - Build and run the server with default.conf"
	@echo "  valgrind    - Run with Valgrind memory checker"
	@echo "  test        - Run basic tests"
	@echo "  test-suite  - Run comprehensive test suite (automated)"
	@echo "  test-stress - Run stress test with Siege (50 concurrent users)"
	@echo "  test-memory - Run test suite under Valgrind (memory leaks)"
	@echo "  debug       - Build with debug flags and sanitizers"
	@echo "  help        - Show this help message"

.PHONY: all clean fclean re run valgrind test test-suite test-stress test-memory debug help