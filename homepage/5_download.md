---
layout: default
title: HS5 - Download and run HS5
nav_title: Download
group: "navigation"
permalink: download.html
---

<script type="text/javascript">
window.onload_fun = function()
{
	if(typeof location.hash!="undefined" && location.hash.length>1)
	{
		var elem = document.getElementById(location.hash.substr(1));
		if(elem)
		{
			elem.style.color='red';
		}
	}
}

function markHeading()
{
	setTimeout(markHeadingExec, 10);
	return true;
}

function markHeadingExec()
{
	var headings = ["linux", "docker"];
	for(var i=0;i<headings.length;++i)
	{
		var elem = document.getElementById(headings[i]);
		if(elem)
		{
			elem.style.color='black';
		}
	}
	if(typeof location.hash!="undefined" && location.hash.length>1)
	{
		var elem = document.getElementById(location.hash.substr(1));
		if(elem)
		{
			elem.style.color='red';
		}
	}
	
	return true;
}
</script>

# Downloads
<ul>
<li><a href="#linux" onClick="return markHeading()">Linux</a></li>
<li><a href="#docker" onClick="return markHeading()">Docker</a></li>
</ul>

---

<h2 id="linux">Linux</h2>

TBD

<h2 id="docker">Docker</h2>

TBD

## &ensp;
## &ensp;