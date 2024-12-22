---
layout: default
title: UrBackup - News archive
---

# News archive

<ul>
{% for post in site.tags.news %}
<li>
<span>
<time datetime="{{ post.date | date: "%Y-%m-%d" }}">
{{ post.date | date: "%Y-%m-%d" }}
</time>
&emsp;</span>
<a href="{{ post.url }}">
{{ post.title }}
</a>
</li>
{% endfor %}
</ul>
