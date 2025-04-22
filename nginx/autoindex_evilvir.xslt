<?xml version="1.0" encoding="UTF-8" ?>
<xsl:stylesheet version="1.0" xmlns:xsl="http://www.w3.org/1999/XSL/Transform" xmlns:D="DAV:" exclude-result-prefixes="D">
<xsl:output method="html" encoding="UTF-8" />

<xsl:template match="D:multistatus">
    <xsl:text disable-output-escaping="yes">&lt;?xml version="1.0" encoding="utf-8" ?&gt;</xsl:text>
    <D:multistatus xmlns:D="DAV:">
        <xsl:copy-of select="*"/>
    </D:multistatus>
</xsl:template>

<!-- 内联 SVG 图标库 -->
<xsl:template name="svg-icons">
  <svg xmlns="http://www.w3.org/2000/svg" style="display:none;">
    <!-- 文件夹图标 -->
    <symbol id="icon-folder" viewBox="0 0 24 24">
      <path d="M20 5h-8.586l-2-2H4c-1.104 0-2 .896-2 2v14c0 1.104.896 2 2 2h16c1.104 0 2-.896 2-2V7c0-1.104-.896-2-2-2zm0 14H4V7h6.586l2 2H20v10z"/>
    </symbol>
    
    <!-- 文件图标 -->
    <symbol id="icon-file" viewBox="0 0 24 24">
      <path d="M14 2H6c-1.1 0-2 .9-2 2v16c0 1.1.9 2 2 2h12c1.1 0 2-.9 2-2V8l-6-6zM6 20V4h7v5h5v11H6z"/>
    </symbol>
    
    <!-- 向上箭头 -->
    <symbol id="icon-arrow-up" viewBox="0 0 24 24">
      <path d="M12 3.975L4.075 11.9 5.9 13.725 11 8.625V20h2V8.625l5.1 5.1 1.825-1.825L12 3.975z"/>
    </symbol>
    
    <!-- 主页图标 -->
    <symbol id="icon-home" viewBox="0 0 24 24">
      <path d="M12 2L2 12h3v8h6v-6h2v6h6v-8h3L12 2zm0 5.5l5.5 5.5H14v6h-4v-6H6.5L12 7.5z"/>
    </symbol>
  </svg>
</xsl:template>

<xsl:template match="/list">
  <xsl:text disable-output-escaping="yes">&lt;!DOCTYPE html&gt;</xsl:text>
  <html>
    <head>
    <script type="text/javascript"><![CDATA[
        document.addEventListener('DOMContentLoaded', function(){ 
            function calculateSize(size){
                var sufixes = ['B', 'KB', 'MB', 'GB', 'TB'];
                var output = size;
                var q = 0;

                while (size / 1024 > 1){
                    size = size / 1024;
                    q++;
                }
                return (Math.round(size * 100) / 100) + ' ' + sufixes[q];
            }
    
            if (window.location.pathname == '/'){
                document.querySelector('.directory.go-up').style.display = 'none';
            }

            var path = window.location.pathname.split('/');
            var nav = document.querySelector("nav#breadcrumbs ul");
            var pathSoFar = '';
        
            for (var i=1; i<path.length-1; i++){
                pathSoFar += '/' + decodeURI(path[i]);
                nav.innerHTML += '<li><a href="' + encodeURI(pathSoFar)  + '">' + decodeURI(path[i]) + '</a></li>';
            }

            var mtimes = document.querySelectorAll("table#contents td.mtime a");
            for (var i=0; i<mtimes.length; i++){
                var mtime = mtimes[i].textContent;
                if (mtime){
                    var d = new Date(mtime);
                    mtimes[i].textContent = d.toLocaleString();
                }
            }

            var sizes = document.querySelectorAll("table#contents td.size a");
            for (var i=0; i<sizes.length; i++){
                var size = sizes[i].textContent;
                if (size){
                    sizes[i].textContent = calculateSize(parseInt(size));
                }
            }
        }, false);
    ]]></script>

	<script type="text/javascript"><![CDATA[
	// 文件上传处理函数
	function handleFileUpload(file) {
		var formData = new FormData();
		formData.append('file', file); // 字段名必须与Nginx配置匹配

		var xhr = new XMLHttpRequest();
		xhr.open('POST', '/upload', true);

		// 进度条更新
		xhr.upload.onprogress = function(e) {
			if (e.lengthComputable) {
				var percent = Math.round((e.loaded / e.total) * 100);
				document.getElementById('upload-progress').style.width = percent + '%';
			}
		};

		// 状态变化回调
		xhr.onreadystatechange = function() {
			if (xhr.readyState === 4) {
				document.getElementById('upload-progress').style.width = '0%';
				if (xhr.status === 200) {
					console.log('上传成功:', xhr.responseText);
					location.reload(); // 刷新页面
				} else {
					console.error('上传失败:', xhr.status, xhr.statusText);
					alert('上传失败，请检查文件大小或网络连接');
				}
			}
		};

		// 发送请求
		xhr.send(formData);
	}

	// 初始化上传组件
	document.addEventListener('DOMContentLoaded', function() {
		// 创建上传容器
		var uploadContainer = document.createElement('div');
		uploadContainer.id = 'upload-section';
		
		// 文件输入元素
		var uploadInput = document.createElement('input');
		uploadInput.type = 'file';
		uploadInput.id = 'mp3-upload';
		uploadInput.accept = 'audio/mpeg';
		uploadInput.style.display = 'none'; // 隐藏输入
		
		// 标签触发按钮
		var uploadLabel = document.createElement('label');
		uploadLabel.htmlFor = 'mp3-upload';
		uploadLabel.className = 'upload-button';
		uploadLabel.innerHTML = `
			<span class="icon">
				<svg><use href="#icon-file"/></svg>
			</span>
			<span class="button-text">上传MP3文件</span>
		`;
		
		// 进度条
		var progressBar = document.createElement('div');
		progressBar.id = 'upload-progress';
		
		// 组装组件
		uploadContainer.appendChild(uploadInput);
		uploadContainer.appendChild(uploadLabel);
		uploadContainer.appendChild(progressBar);
		
		// 插入到导航栏和表格之间
		var nav = document.querySelector('nav#breadcrumbs');
		if (nav && nav.parentNode) {
			nav.parentNode.insertBefore(uploadContainer, nav.nextSibling);
		} else {
			document.body.prepend(uploadContainer);
		}

		// 绑定文件选择事件
		uploadInput.addEventListener('change', function(e) {
			var files = e.target.files;
			if (files.length > 0) {
				handleFileUpload(files[0]);
				e.target.value = ''; // 清除选择以便重复上传
			}
		});
	});
	]]></script>

    <style type="text/css"><![CDATA[
        * { box-sizing: border-box; }
        html { margin: 0px; padding: 0px; height: 100%; width: 100%; }
        body { background-color: #303030; font-family: Verdana, Geneva, sans-serif; font-size: 14px; padding: 20px; margin: 0px; height: 100%; width: 100%; }

        table#contents td a { text-decoration: none; display: block; padding: 10px 30px 10px 30px; }
        table#contents { width: 50%; margin-left: auto; margin-right: auto; border-collapse: collapse; border-width: 0px; }
        table#contents td { padding: 0px; word-wrap: none; white-space: nowrap; }
        table#contents td.icon, table td.size, table td.mtime { width: 1px; white-space: nowrap; }
        table#contents td.icon a { padding-left: 0px; padding-right: 5px; }
        table#contents td.name a { padding-left: 5px; }
        table#contents td.size a { color: #9e9e9e }
        table#contents td.mtime a { padding-right: 0px; color: #9e9e9e }
        table#contents tr * { color: #efefef; }
        table#contents tr:hover * { color: #c1c1c1 !important; }
        table#contents tr.directory td.icon i { color: #FBDD7C !important; }
        table#contents tr.directory.go-up td.icon i { color: #BF8EF3 !important; }

        nav#breadcrumbs { margin-bottom: 50px; display: flex; justify-content: center; align-items: center; }
        nav#breadcrumbs ul { list-style: none; display: inline-block; margin: 0px; padding: 0px; }
        nav#breadcrumbs ul li { float: left; }
        nav#breadcrumbs ul li a { 
            color: #FFF; display: block; 
            background: #515151; 
            text-decoration: none; 
            position: relative; 
            height: 40px; 
            line-height: 40px; 
            padding: 0 10px 0 5px; 
            margin-right: 23px; 
        }
    ]]></style>
	
	<style type="text/css"><![CDATA[
	  /* 确保上传标签按钮可见 */
		#upload-section {
		  text-align: center;
		  margin: 20px auto;
		  display: flex;          /* 新增 */
		  flex-direction: column; /* 新增 */
		  align-items: center;    /* 新增 */
		}
	  
	  .upload-button {
		  display: inline-flex !important;
		  align-items: center;
		  padding: 12px 25px;
		  background: #515151;
		  color: #FBDD7C !important;
		  border-radius: 4px;
		  cursor: pointer;
		  transition: all 0.3s;
		  border: 1px solid #6d6d6d;
	  }
	  
	  .upload-button:hover {
		  background: #6d6d6d;
		  color: #FFE79E !important;
	  }
	  
	  #upload-progress {
		  height: 3px;
		  background: #FBDD7C;
		  width: 0%;
		  margin-top: 5px;
		  transition: width 0.3s ease;
	  }
	  
	  /* 修复图标对齐 */
	  .upload-button .icon svg {
		  width: 18px;
		  height: 18px;
		  vertical-align: middle;
		  margin-right: 8px;
	  }
	]]></style>

	<style type="text/css"><![CDATA[
	  /* 新增上传按钮图标尺寸控制 */
	  #upload-section .icon svg {
		width: 1.2em;       /* 与文字大小比例协调 */
		height: 1.2em;
		vertical-align: -0.2em;  /* 优化图标与文字对齐 */
	  }

	  /* 覆盖面包屑导航的全局影响 */
	  #upload-section .icon {
		font-size: 14px;    /* 基于基础字体尺寸设定 */
		line-height: 1.5;   /* 保持行高统一 */
	  }
	]]></style>

	<style type="text/css"><![CDATA[
	  /* 修改上传按钮容器为flex布局 */
	  .upload-button {
		display: inline-flex !important;
		align-items: center;  /* 垂直居中 */
		justify-content: center; /* 水平居中 */
		padding: 12px 25px !important;
	  }

	  /* 图标尺寸微调 */
	  .upload-button .icon {
		width: 18px;
		height: 18px;
		margin-right: 8px; /* 图标文字间距 */
		position: relative;
		top: -1px; /* 视觉补偿微调 */
	  }

	  /* 确保SVG充满容器 */
	  .upload-button .icon svg {
		width: 100%;
		height: 100%;
		vertical-align: top; /* 消除SVG默认基线对齐 */
	  }
	]]></style>
	
	<style type="text/css"><![CDATA[
		  /* 导航栏图标特殊调整 */
		nav#breadcrumbs .icon svg {
			width: 2.0em;       /* 缩小尺寸 */
			height: 2.0em;
			vertical-align: middle;  /* 优化对齐 */
		}
		
        /* 图标基础样式 */
        .icon svg {
          width: 1em;
          height: 1em;
          vertical-align: -0.15em;
          fill: currentColor;
        }

        /* 颜色覆盖 */
        tr.directory td.icon svg { color: #FBDD7C; }
        tr.directory.go-up td.icon svg { color: #BF8EF3; }
        tr.file td.icon svg { color: #efefef; }

        /* 原样保留其他样式 */
        table#contents td a { text-decoration: none; display: block; padding: 10px 30px 10px 30px; }
        /* ... 其他原有样式保持不变 ... */
    ]]></style>
	
    </head>
    <body>
	  <xsl:call-template name="svg-icons"/> <!-- 插入图标库 -->
	
      <div>
        <nav id="breadcrumbs">
          <ul>
            <li>
              <a href="/">
				<span class="icon">  <!-- 添加图标容器 -->
					<svg><use href="#icon-home"/></svg>
				</span>
              </a>
            </li>
          </ul>
        </nav>
		
          <table id="contents">
            <tbody>
                <tr class="directory go-up">
              <td class="icon">
                <a href="../">
                  <svg><use href="#icon-arrow-up"/></svg>
                </a>
              </td>
                  <td class="name"><a href="../">..</a></td>
                  <td class="size"><a href="../"></a></td>
                  <td class="mtime"><a href="../"></a></td>
                </tr>
            
              <xsl:if test="count(directory) != 0">
                <tr class="separator directories">
                  <td colspan="4"><hr/></td>
                </tr>
              </xsl:if>

              <xsl:for-each select="directory">
                <tr class="directory">
                <td class="icon">
                  <a href="{.}/">
                    <svg><use href="#icon-folder"/></svg>
                  </a>
                </td>
                  <td class="name"><a href="{.}/"><xsl:value-of select="." /></a></td>
                  <td class="size"><a href="{.}/"></a></td>
                  <td class="mtime"><a href="{.}/"><xsl:value-of select="./@mtime" /></a></td>
                </tr>
              </xsl:for-each>

              <xsl:if test="count(file) != 0">
                <tr class="separator files">
                  <td colspan="4"><hr/></td>
                </tr>
              </xsl:if>

              <xsl:for-each select="file">
			    <xsl:sort select="@mtime" order="descending"/>
                <tr class="file">
                <td class="icon">
                  <a href="{.}" download="{.}">
                    <svg><use href="#icon-file"/></svg>
                  </a>
                </td>
                  <td class="name"><a href="{.}" download="{.}"><xsl:value-of select="." /></a></td>
                  <td class="size"><a href="{.}" download="{.}"><xsl:value-of select="./@size" /></a></td>
                  <td class="mtime"><a href="{.}" download="{.}"><xsl:value-of select="./@mtime" /></a></td>
                </tr>
              </xsl:for-each>
            </tbody>
          </table>
      </div>
    </body>
  </html>
</xsl:template>
</xsl:stylesheet>