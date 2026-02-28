--- install.sh
+++ install.sh
@@ -155,8 +155,11 @@
         screen -r "\$SCREEN_NAME"
         ;;
     *)
-        echo "Usage: saia {start|stop|restart|status|attach}"
-        exit 1
+        if ! screen -list | grep -q "\$SCREEN_NAME"; then
+            \$0 start
+            sleep 1
+        fi
+        \$0 attach
         ;;
 esac
 EOF
@@ -176,7 +179,10 @@
     create_wrapper
     setup_autostart
     "$INSTALL_DIR/saia_manager.sh" start
-    log "Installation Complete! Type 'saia attach' to see the menu."
+    
+    echo -e "\n${YELLOW}================================================${NC}"
+    echo -e "${GREEN}安装完成！程序已在后台守护运行。${NC}"
+    echo -e "${YELLOW}由于您正在执行一键脚本，请在终端手动输入以下命令来打开菜单：${NC}\n"
+    echo -e "    ${GREEN}source ~/.bashrc && saia${NC}\n"
+    echo -e "${YELLOW}以后无论何时，只要在终端输入 ${GREEN}saia${YELLOW} 即可直接呼出菜单！${NC}"
+    echo -e "${YELLOW}================================================${NC}\n"
 }
 
 main
