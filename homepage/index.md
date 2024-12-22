---
layout: default
title: HS5 - High-performance scale-up self-hosted simple storage service
---
{% row %}

{% col 2-3 %}

## What is HS5?

High-performance scale-up self-hosted simple storage service (hs5)

 * High performance: Designed to run with high performance

 * Scale-up: Runs only on a single node. To scale it use it on a better machine. With machines with terabytes of RAM and hundreds of terabytes of storage available this might be enough for many use cases

 * Self-hosted: You run it yourself, keeping ownership and responsibility of your data

 * Simple: Simple to setup and run. API-compatible with AWS S3 API

 * Storage Service: An object storage service like AWS S3


		
{% endcol %}

{% col 1-3 %}

## &emsp;

<span class="button">
	<a href="{{site.dlurl}}/Client/{{site.curr_client_version}}/UrBackup%20Client%20{{site.curr_client_version}}.exe">
		Download UrBackup Client<br />
		<small>{{ site.curr_client_version }} (Windows)</small>
	</a>
</span>

<span class="button">
	<a href="{{site.dlurl}}/Server/{{site.curr_server_version}}/UrBackup%20Server%20{{site.curr_server_version}}.exe">
		Download UrBackup Server<br />
		<small>{{ site.curr_server_version }} (Windows)</small>
	</a>
</span>

<span class="button">
	<a href="{{site.dlurl}}/Restore/urbackup_restore_{{site.curr_restore_version_amd64}}.iso">
		Download UrBackup Restore CD<br />
		<small>{{site.curr_restore_version_amd64}} (x64)</small>
	</a>
</span>

<span class="button">
	<a href="download.html">
		Other download options<br />
		<small>FreeBSD, Linux, etc.</small>
	</a>
</span>

{% endcol %}

{% endrow %}

---

{% row %}

{% col 1-2 %}
<table style="margin-top: 15px"><tr>
<td style="vertical-align: top; padding: 10px; width: 64px"><img src="/images/Runner.png" /></td>
<td style="padding-left: 0.3em"><strong>Fast.</strong> TBD</td>
</tr></table>
{% endcol %}

{% col 1-2 %}
<table style="margin-top: 15px"><tr>
<td style="vertical-align: top; padding: 10px; width: 64px"><img src="/images/Dashboard.png" /></td>
<td style="padding-left: 0.3em"><strong>Web interface.</strong> TBD</td>
</tr></table>
{% endcol %}

{% endrow %}
{% row %}

{% col 1-2 %}
<table style="margin-top: 15px"><tr>
<td style="vertical-align: top; padding: 10px; width: 64px"><img src="/images/Setup.png" /></td>
<td style="padding-left: 0.3em"><strong>Easy to setup.</strong> TBD</td>
</tr></table>
{% endcol %}

{% endrow %}


{% row %}

{% col 1-2 %}
<table style="margin-top: 15px"><tr>
<td style="vertical-align: top; padding: 10px; width: 64px"><img src="/images/osi_logo.png" /></td>
<td style="padding-left: 0.3em"><strong>Free Software.</strong> HS5 is Free Open Source software licensed under the OSI certified <a href="https://www.gnu.org/licenses/lgpl-3.0.en.html">LGPLv3+</a>. As long as you adhere to the terms and conditions of the license you are allowed to use and redistribute HS5 in a personal and commercial setting.</td>
</tr></table>
{% endcol %}

{% endrow %}

---

## Getting started

TBD
---

## News

<div class="pure-g-r">

	{% for post in site.tags.news limit:3 %}
		<div class="pure-u-1-3">
			<div style="padding: 0.4em">
				<h3>{{post.date | date: "%Y-%m-%d"}}:<br />{{ post.title }}</h3>
				{{ post.content }}
			</div>
		</div>
	{% endfor %}

</div>

See the [news archive](news.html) for older news.