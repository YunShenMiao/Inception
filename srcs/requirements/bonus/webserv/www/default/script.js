// Simple Markdown Parser
function parseMarkdown(markdown) {
  if (!markdown) return '';
  
  let html = markdown;
  
  // Escape HTML to prevent XSS
  const escapeHtml = (text) => {
    const div = document.createElement('div');
    div.textContent = text;
    return div.innerHTML;
  };
  
  // Store code blocks temporarily to prevent processing
  const codeBlocks = [];
  html = html.replace(/```(\w*)\n([\s\S]*?)```/g, (match, lang, code) => {
    const index = codeBlocks.length;
    codeBlocks.push({ lang, code: escapeHtml(code.trim()) });
    return `%%CODEBLOCK${index}%%`;
  });

  const splitTableRow = (row) => {
    return row
      .trim()
      .replace(/^\|/, '')
      .replace(/\|$/, '')
      .split('|')
      .map((cell) => cell.trim());
  };

  const getTableAlignment = (cell) => {
    if (/^:-+:$/.test(cell)) return 'center';
    if (/^-+:$/.test(cell)) return 'right';
    if (/^:-+$/.test(cell)) return 'left';
    return '';
  };

  const isTableSeparator = (line) => {
    const cells = splitTableRow(line);
    return cells.length > 0 && cells.every((cell) => /^:?-{3,}:?$/.test(cell));
  };

  const renderTable = (lines) => {
    const headerCells = splitTableRow(lines[0]);
    const alignments = splitTableRow(lines[1]).map(getTableAlignment);
    const bodyRows = lines.slice(2);

    const renderCells = (cells, tagName) => {
      return cells.map((cell, index) => {
        const align = alignments[index] ? ` style="text-align: ${alignments[index]};"` : '';
        return `<${tagName}${align}>${cell}</${tagName}>`;
      }).join('');
    };

    const thead = `<thead><tr>${renderCells(headerCells, 'th')}</tr></thead>`;
    const tbody = bodyRows.length
      ? `<tbody>${bodyRows.map((row) => `<tr>${renderCells(splitTableRow(row), 'td')}</tr>`).join('')}</tbody>`
      : '';

    return `<div class="table-wrapper"><table>${thead}${tbody}</table></div>`;
  };
  
  // Inline code
  html = html.replace(/`([^`]+)`/g, '<code>$1</code>');
  
  // Headers
  html = html.replace(/^### (.+)$/gm, '<h3>$1</h3>');
  html = html.replace(/^## (.+)$/gm, '<h2>$1</h2>');
  html = html.replace(/^# (.+)$/gm, '<h1>$1</h1>');
  
  // Bold and Italic
  html = html.replace(/\*\*\*(.+?)\*\*\*/g, '<strong><em>$1</em></strong>');
  html = html.replace(/\*\*(.+?)\*\*/g, '<strong>$1</strong>');
  html = html.replace(/\*(.+?)\*/g, '<em>$1</em>');
  
  // Links
  html = html.replace(/\[([^\]]+)\]\(([^)]+)\)/g, '<a href="$2" target="_blank" rel="noopener noreferrer">$1</a>');
  
  // Blockquotes
  html = html.replace(/^> (.+)$/gm, '<blockquote>$1</blockquote>');
  
  // Horizontal rules
  html = html.replace(/^---$/gm, '<hr>');

  // Tables
  const tableBlocks = [];
  const lines = html.split('\n');
  const processedLines = [];

  for (let i = 0; i < lines.length; i++) {
    const line = lines[i].trim();

    if (
      line.startsWith('|') &&
      i + 1 < lines.length &&
      isTableSeparator(lines[i + 1].trim())
    ) {
      const tableLines = [lines[i], lines[i + 1]];
      i += 2;

      while (i < lines.length && lines[i].trim().startsWith('|')) {
        tableLines.push(lines[i]);
        i++;
      }

      i--;
      const index = tableBlocks.length;
      tableBlocks.push(renderTable(tableLines));
      processedLines.push(`%%TABLE${index}%%`);
      continue;
    }

    processedLines.push(lines[i]);
  }

  html = processedLines.join('\n');
  
  // Unordered lists
  html = html.replace(/^[\-\*] (.+)$/gm, '<li>$1</li>');
  html = html.replace(/(<li>.*<\/li>)\n(?=<li>)/g, '$1');
  html = html.replace(/(<li>[\s\S]*?<\/li>)/g, '<ul>$1</ul>');
  html = html.replace(/<\/ul>\n<ul>/g, '\n');
  
  // Ordered lists
  html = html.replace(/^\d+\. (.+)$/gm, '<oli>$1</oli>');
  html = html.replace(/(<oli>.*<\/oli>)\n(?=<oli>)/g, '$1');
  html = html.replace(/(<oli>[\s\S]*?<\/oli>)/g, (match) => {
    return '<ol>' + match.replace(/<\/?oli>/g, (tag) => tag.replace('oli', 'li')) + '</ol>';
  });
  html = html.replace(/<\/ol>\n<ol>/g, '\n');
  
  // Paragraphs
  html = html.replace(/\n\n(?!<[huplob])/g, '</p><p>');
  html = '<p>' + html + '</p>';
  html = html.replace(/<p><\/p>/g, '');
  html = html.replace(/<p>(<[huplob])/g, '$1');
  html = html.replace(/(<\/[huplob][^>]*>)<\/p>/g, '$1');

  // Restore tables before the final cleanup so wrappers don't end up inside paragraphs
  tableBlocks.forEach((table, index) => {
    html = html.replace(`%%TABLE${index}%%`, table);
  });
  html = html.replace(/<p>\s*(<div class="table-wrapper">[\s\S]*?<\/div>)\s*<\/p>/g, '$1');
  
  // Restore code blocks
  codeBlocks.forEach((block, index) => {
    const langClass = block.lang ? ` class="language-${block.lang}"` : '';
    html = html.replace(`%%CODEBLOCK${index}%%`, `<pre><code${langClass}>${block.code}</code></pre>`);
  });
  
  // Clean up empty paragraphs and extra whitespace
  html = html.replace(/<p>\s*<\/p>/g, '');
  html = html.replace(/<p>(\s*<(?:h[1-6]|ul|ol|pre|blockquote|hr|div))/g, '$1');
  html = html.replace(/(<\/(?:h[1-6]|ul|ol|pre|blockquote|hr|div)>\s*)<\/p>/g, '$1');
  
  return html;
}

// DOM Elements
const docsBtn = document.getElementById('docsBtn');
const modalOverlay = document.getElementById('modalOverlay');
const modalClose = document.getElementById('modalClose');
const docsContent = document.getElementById('docsContent');

// Markdown content for documentation
let docsMarkdown = 'Loading...';

async function fetchDocsMarkdown() {
  try {
    const response = await fetch('https://raw.githubusercontent.com/YunShenMiao/CatSurf/1810f67bb0a871640c9079239178e1347fe8ecde/README.md');
    docsMarkdown = await response.text();
  } catch {
    docsMarkdown = 'Failed to load documentation.';
  }
}

// Fetch the markdown as soon as possible
fetchDocsMarkdown();

// Open modal
function openModal() {
  // Render markdown content
  docsContent.innerHTML = `<div class="markdown">${parseMarkdown(docsMarkdown)}</div>`;
  modalOverlay.classList.add('active');
  document.body.style.overflow = 'hidden';
  
  // Focus management for accessibility
  modalClose.focus();
}

// Close modal
function closeModal() {
  modalOverlay.classList.remove('active');
  document.body.style.overflow = '';
  docsBtn.focus();
}

// Event Listeners
docsBtn.addEventListener('click', openModal);
modalClose.addEventListener('click', closeModal);

// Close on overlay click
modalOverlay.addEventListener('click', (e) => {
  if (e.target === modalOverlay) {
    closeModal();
  }
});

// Close on Escape key
document.addEventListener('keydown', (e) => {
  if (e.key === 'Escape' && modalOverlay.classList.contains('active')) {
    closeModal();
  }
});

// Trap focus within modal
modalOverlay.addEventListener('keydown', (e) => {
  if (e.key === 'Tab') {
    const focusableElements = modalOverlay.querySelectorAll(
      'button, [href], input, select, textarea, [tabindex]:not([tabindex="-1"])'
    );
    const firstElement = focusableElements[0];
    const lastElement = focusableElements[focusableElements.length - 1];
    
    if (e.shiftKey && document.activeElement === firstElement) {
      e.preventDefault();
      lastElement.focus();
    } else if (!e.shiftKey && document.activeElement === lastElement) {
      e.preventDefault();
      firstElement.focus();
    }
  }
});
