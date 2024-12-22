# From: https://github.com/glfw/website/blob/master/_plugins/grids.rb
# Author: siavashserver
# Jekyll helper plugin for using Pure CSS grids
#
# This plugin extends Liquid tag blocks with a few custom tags to avoid
# mixing Markdown and HTML syntax directly.
#
# NOTE:
# This plugin uses Maruku, Jekyll's default markdown syntax parser.
# Make sure it is enabled as default markdown syntax parser to avoid
# inconsistency between generated HTML code.
#
# Sample usage:
#
# {% row %}
# {% col 1-3 %}
#    One-Third Width
# {% endcol %}
# {% col 2-3 %}
#    Two-Third Width
# {% endcol %}
# {% endrow %}
#
# Usage Note:
# If you need normal behaviour (non-responsive) pass "regular"
# {% row regular %}

module Jekyll
  class ResponsiveRow < Liquid::Block
    def initialize(tag_name, text, tokens)
      super
      if text.strip == 'regular'
        @responsive = 'g'
      else
        @responsive = 'g-r'
      end
    end

    def render(context)
      contents = super
      %|<div class="pure-#{@responsive}">#{contents}</div>|
    end
  end

  class ResponsiveColumn < Liquid::Block
    def initialize(tag_name, text, tokens)
      super
      @width = text
    end

    def render(context)
      require 'kramdown'
      contents = super
      %|<div class="pure-u-#{@width}"><div class="box">#{Kramdown::Document.new(contents).to_html}</div></div>|
    end
  end
end

Liquid::Template.register_tag('row', Jekyll::ResponsiveRow)
Liquid::Template.register_tag('col', Jekyll::ResponsiveColumn)
