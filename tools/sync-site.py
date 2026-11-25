"""Update the checked-in home pages and shared chrome. Serving needs no build."""

import argparse
from html import escape
from pathlib import Path
import re


ROOT = Path(__file__).resolve().parent.parent
REPO = "https://github.com/brunolimawd/grkit-light-leak"
RELEASES = f"{REPO}/releases"
GRKIT = "https://grkit.app/"
GRKIT_MAINTAINER = "https://github.com/ahonn"
RECOVERY = "https://grkit.app/docs/recovery/"
PHOTO = "https://www.flickr.com/photos/mujitra/53311001084/"
PHOTO_LICENSE = "https://creativecommons.org/licenses/by/2.0/"
GTM_ID = "GTM-52MK6S86"
GTM_HEAD = """  <!-- Google Tag Manager -->
  <script>(function(w,d,s,l,i){w[l]=w[l]||[];w[l].push({'gtm.start':
  new Date().getTime(),event:'gtm.js'});var f=d.getElementsByTagName(s)[0],
  j=d.createElement(s),dl=l!='dataLayer'?'&l='+l:'';j.async=true;j.src=
  'https://www.googletagmanager.com/gtm.js?id='+i+dl;f.parentNode.insertBefore(j,f);
  })(window,document,'script','dataLayer','%s');</script>
  <!-- End Google Tag Manager -->""" % GTM_ID
GTM_BODY = f"""  <!-- Google Tag Manager (noscript) -->
  <noscript><iframe src="https://www.googletagmanager.com/ns.html?id={GTM_ID}"
  height="0" width="0" style="display:none;visibility:hidden" title="Google Tag Manager"></iframe></noscript>
  <!-- End Google Tag Manager (noscript) -->"""
STYLES = (
    ("Slit Flare", "slit-flare", "2-slit"),
    ("Burnt Edge", "burnt-edge", "3-burnt"),
    ("Film Band", "film-band", "4-band"),
    ("Amber Veil", "amber-veil", "5-veil"),
    ("Prismatic Arc", "prismatic-arc", "6-prism"),
    ("Color Fog", "color-fog", "7-fog"),
)
COPY = {
    "en": {
        "name": "English",
        "language": "Language",
        "nav": "Main navigation",
        "links": ("Looks", "Install", "FAQ", "Guide"),
        "help": ("Help", "Compatibility", "Recovery"),
        "project": ("Project", "Releases", "Report an issue", "GRKit project"),
        "legal": ("Legal", "Photo and design credits", "MIT license"),
        "disclaimer": "Light Leak is an independent module maintained by brunolimawd. It is not part of the official GRKit distribution and is not affiliated with Ricoh.",
        "lead": ("Film light leaks for your RICOH GR.", "No firmware flashing."),
        "summary": "An unofficial module written to your SD card. Tested on the RICOH GR III with firmware 2.10. Applies to JPEGs and previews without changing RAW files.",
        "upstream": ("Original project", "Visit GRKit", "Light Leak is an independent module for {grkit}. Thanks to {maintainer} for creating and maintaining GRKit, and for the dedication that makes modules like this possible."),
        "photo": "A food truck with a striped awning and customers outside",
        "compare": "Before and after: slide to reveal the Light Leak effect",
        "before": "Without Light Leak",
        "demo": "Off-camera demo",
        "preview": "The looks above use Medium strength, Original position and Variation 1.",
        "controls": ("Strength", "Choose how much light enters the frame. Try Slit Flare at three strengths on the same photo.", ("Weak", "Medium", "Strong"), "JPEG"),
        "details": ("More controls", "Leak Position", "Choose the edge the light enters from. Original uses each look’s default side.", ("Right", "Left", "Top", "Bottom"), "Variation", "Change the shape and colors without switching styles. Variation 1 is the original look.", ("1", "2", "3")),
        "camera": ("On the camera", "Menu diagram", "Press ADJ, choose GRK and open Light Leak.", "Press Fn in Light Leak to change the settings."),
        "faq": (
            ("Is Light Leak part of GRKit?", "No. It is a separate, unofficial module. A compatible GRKit runtime must already be installed."),
            ("Which cameras have been tested?", "The module was tested on the RICOH GR III with firmware 2.10. The GR IIIx used for the sample photo and other models have not been tested with the module."),
            ("Does it change RAW files?", "No. Light Leak is written to the JPEG and its previews, not to the RAW sensor data."),
            ("Can I see the effect before shooting?", "No. The viewfinder icon indicates the active style, but the effect itself appears only in the recorded JPEG and its previews."),
            ("Where do I put LITLEAK.GRM?", "Copy it to <code>GRKIT/MODULES/LITLEAK.GRM</code> from the root of the SD card."),
            ("How do I remove it?", "Turn your camera off and remove <code>GRKIT/MODULES/LITLEAK.GRM</code> from the card. You do not need to flash the firmware."),
        ),
        "faq-title": "Before you install",
        "safe": ("Safe to try", ("No firmware flashing", "The module loads from the SD card. The camera firmware stays as it is."), ("Easy to remove", "Delete the module file from the card to uninstall Light Leak."), ("RAW files stay intact", "The effect is applied only to JPEGs and previews.")),
        "browse": "Read about all six styles in the guide",
    },
    "pt-BR": {
        "name": "Português",
        "language": "Idioma",
        "nav": "Navegação principal",
        "links": ("Estilos", "Instalar", "Dúvidas", "Guia"),
        "help": ("Ajuda", "Compatibilidade", "Recuperação"),
        "project": ("Projeto", "Versões", "Reportar problema", "Projeto GRKit"),
        "legal": ("Legal", "Créditos da foto e do design", "Licença MIT"),
        "disclaimer": "Light Leak é um módulo independente mantido por brunolimawd. Não faz parte da distribuição oficial do GRKit e não é afiliado à Ricoh.",
        "lead": ("Vazamentos de luz de filme na sua RICOH GR.", "Sem gravar firmware."),
        "summary": "Um módulo não oficial que roda do cartão SD. Testado na RICOH GR III com firmware 2.10. O efeito sai no JPEG e nas prévias, sem alterar o RAW.",
        "upstream": ("Projeto original", "Visitar o GRKit", "O Light Leak é um módulo independente para o {grkit}. Obrigado a {maintainer} pela dedicação em criar e manter o GRKit e por tornar possíveis módulos como este."),
        "photo": "Um food truck com toldo listrado e clientes do lado de fora",
        "compare": "Antes e depois: deslize para revelar o efeito Light Leak",
        "before": "Sem Light Leak",
        "demo": "Demonstração fora da câmera",
        "preview": "Os estilos acima usam intensidade média, posição original e variação 1.",
        "controls": ("Intensidade", "Escolha quanta luz entra no quadro. Compare o Slit Flare em três intensidades na mesma foto.", ("Fraca", "Média", "Forte"), "JPEG"),
        "details": ("Mais controles", "Posição", "Escolha a borda por onde a luz entra. Original usa o lado padrão de cada estilo.", ("Direita", "Esquerda", "Topo", "Baixo"), "Variação", "Mude a forma e as cores sem trocar de estilo. A variação 1 mantém o visual original.", ("1", "2", "3")),
        "camera": ("Na câmera", "Diagrama do menu", "Pressione ADJ, escolha GRK e abra Light Leak.", "Pressione Fn no Light Leak para mudar as opções."),
        "faq": (
            ("O Light Leak faz parte do GRKit?", "Não. É um módulo separado e não oficial. É preciso instalar antes uma versão compatível do GRKit."),
            ("Quais câmeras foram testadas?", "O módulo foi testado na RICOH GR III com firmware 2.10. A GR IIIx usada na foto de exemplo e os demais modelos não foram testados com este módulo."),
            ("O efeito altera os arquivos RAW?", "Não. O Light Leak sai no JPEG e nas prévias, não nos dados brutos do sensor."),
            ("Consigo ver o efeito antes de fotografar?", "Não. O ícone no visor indica o estilo ativo, mas o efeito só aparece no JPEG gravado e em suas prévias."),
            ("Onde coloco o LITLEAK.GRM?", "Copie para <code>GRKIT/MODULES/LITLEAK.GRM</code> a partir da raiz do cartão SD."),
            ("Como removo o módulo?", "Desligue a câmera e remova <code>GRKIT/MODULES/LITLEAK.GRM</code> do cartão. Não precisa gravar o firmware."),
        ),
        "faq-title": "Antes de instalar",
        "safe": ("Instalação reversível", ("Sem gravar firmware", "O módulo roda do cartão SD sem modificar o firmware da câmera."), ("Fácil de remover", "Para desinstalar, apague o arquivo do módulo do cartão."), ("Arquivos RAW intactos", "O efeito é aplicado apenas no JPEG e nas prévias.")),
        "browse": "Conheça os seis estilos no guia",
    },
}


def lang_path(lang, page=""):
    return ("" if lang == "en" else "pt-br/") + (f"{page}/" if page else "")


def link(url, name, current=False):
    active = ' aria-current="page"' if current else ""
    return f'<a href="{escape(url, quote=True)}"{active}>{escape(name)}</a>'


def header(lang, page, root):
    text = COPY[lang]
    other = "pt-BR" if lang == "en" else "en"
    other_url = root + lang_path(other, page)
    home = root + lang_path(lang)
    urls = (home + "#looks", home + "install/", home + "#faq", home + "guide/")
    current = {"install": 1, "guide": 3}.get(page)
    nav = "\n".join(
        f"          {link(url, name, n == current)}"
        for n, (url, name) in enumerate(zip(urls, text["links"]))
    )
    options = "\n".join(
        f'            <option value="{root}{lang_path(code, page)}" lang="{code}"'
        f'{" selected" if code == lang else ""}>{item["name"]}</option>'
        for code, item in COPY.items()
    )
    return f"""  <header class="masthead">
    <div class="masthead-top">
      <div class="masthead-title">
        <a class="masthead-name" href="{home}">Light <span class="kit">Leak</span></a>
        <a class="masthead-version" href="{RELEASES}">beta</a>
      </div>
      <div class="languages">
        <nav class="masthead-nav" aria-label="{text["nav"]}">
{nav}
        </nav>
        <a class="github" href="{REPO}" aria-label="GitHub">
          <img src="{root}assets/github.svg" width="22" height="22" alt="">
        </a>
        <a class="language-fallback" href="{other_url}" lang="{other}" hreflang="{other}">{COPY[other]["name"]}</a>
        <span class="language-select" hidden>
          <select class="button" aria-label="{text["language"]}" data-language>
{options}
          </select>
        </span>
      </div>
    </div>
  </header>"""


def footer(lang, root):
    text = COPY[lang]
    home = root + lang_path(lang)
    looks, install, faq, guide = text["links"]
    help_title, compat, recover = text["help"]
    project, releases, issues, grkit = text["project"]
    legal, credits, license_name = text["legal"]
    groups = (
        ("Light Leak", ((home + "install/", install), (home + "#looks", looks), (REPO, "GitHub"))),
        (help_title, ((home + "guide/", guide), (home + "#faq", faq), (home + "install/#compatibility", compat), (RECOVERY, recover))),
        (project, ((RELEASES, releases), (f"{REPO}/issues", issues), (GRKIT, grkit))),
        (legal, ((f"{REPO}/tree/gh-pages#sample-photo-and-credits", credits), (f"{REPO}/blob/gh-pages/LICENSE", license_name), (root + "assets/IBM-Plex-OFL.txt", "IBM Plex"))),
    )
    columns = []
    for title, items in groups:
        links = "\n".join(f"          <li>{link(url, name)}</li>" for url, name in items)
        columns.append(f"""      <div class="footer-group">
        <p class="footer-heading">{title}</p>
        <ul>
{links}
        </ul>
      </div>""")
    return f"""  <footer class="footer">
    <div class="footer-links">
{chr(10).join(columns)}
    </div>
    <div class="footer-meta">
      <p>{text["disclaimer"]}</p>
      <a href="{RELEASES}">Light Leak · v0.1.0-beta.1</a>
    </div>
  </footer>"""


def home_main(lang, root):
    t = COPY[lang]
    assets = root + "assets/"
    title, intro, forces, jpeg = t["controls"]
    more, position, position_note, sides, variation, variation_note, variants = t["details"]
    camera, diagram, adj, fn = t["camera"]
    safe, *facts = t["safe"]
    install = t["links"][1]
    upstream_title, upstream_link, upstream_credit = t["upstream"]
    upstream_credit = upstream_credit.format(
        grkit=link(GRKIT, "GRKit"),
        maintainer=link(GRKIT_MAINTAINER, "Yuexun Jiang (@ahonn)"),
    )

    def credit(camera=False):
        return f'{link(PHOTO, "MIKI Yoshihito")} · {link(PHOTO_LICENSE, "CC BY 2.0")}' + (
            " · RICOH GR IIIx" if camera else ""
        )

    def image(name, alt, *, responsive=False, target=False, eager=False):
        src = f"{assets}{name}.webp"
        responsive_attrs = ""
        if responsive:
            small = "sample-original-small" if name == "sample-original" else name
            large = name if name == "sample-original" else f"{name}-large"
            src = f"{assets}{large}.webp"
            responsive_attrs = (
                f' srcset="{assets}{small}.webp 600w, {src} 1200w"'
                ' sizes="(max-width: 860px) 92vw, (max-width: 1280px) 48vw, 607px"'
            )
        attrs = (' data-preview-image' if target else '')
        if not eager:
            attrs += ' loading="lazy" decoding="async"'
        elif name == "sample-original":
            attrs += ' fetchpriority="high"'
        return (
            f'<img src="{src}"{responsive_attrs} width="{1200 if responsive else 600}"'
            f' height="{800 if responsive else 400}"{attrs} alt="{escape(alt, quote=True)}">'
        )

    def options(labels, names, selected, mono=False):
        buttons = []
        for idx, (label, name) in enumerate(zip(labels, names)):
            pressed = "true" if selected == idx else "false"
            suffix = " landing-ratio" if mono else ""
            buttons.append(
                f'<button type="button" class="button small{suffix}" aria-pressed="{pressed}"'
                f' data-image="{assets}{name}.webp" data-name="{escape(label, quote=True)}">{escape(label)}</button>'
            )
        return "\n            ".join(buttons)

    tiles = []
    for idx, (name, file, icon) in enumerate(STYLES):
        pressed = "true" if idx == 0 else "false"
        tiles.append(
            f'          <button type="button" class="landing-film" aria-pressed="{pressed}" '
            f'data-name="{name}" data-image="{assets}{file}-large.webp" '
            f'data-srcset="{assets}{file}.webp 600w, {assets}{file}-large.webp 1200w">'
            f'<span class="landing-icon"><img src="{assets}{icon}.png" width="40" height="40" alt=""></span>'
            f'<span lang="en">{name}</span></button>'
        )
    full_tiles = "\n".join(tiles)
    faq = "\n".join(
        f"        <details><summary>{escape(q)}</summary><p>{a}</p></details>"
        for q, a in t["faq"]
    )
    fact_markup = "\n".join(
        f"          <li><h3>{escape(name)}</h3><p>{escape(body)}</p></li>" for name, body in facts
    )

    return f"""  <main id="main" tabindex="-1">
    <section class="landing-hero" id="looks" aria-labelledby="hero-title">
      <div class="landing-copy">
        <h1 class="lead" id="hero-title"><span class="lead-line">{t["lead"][0]}</span> <span class="lead-line">{t["lead"][1]}</span></h1>
        <div class="landing-action">
          <div class="landing-action-links">
            <a class="download landing-install" href="install/">{install}</a>
            <a class="landing-upstream" href="{GRKIT}">{upstream_link}</a>
          </div>
          <p class="note">{t["summary"]}</p>
        </div>
      </div>
      <div class="landing-looks" data-preview data-alt="{escape(t["photo"], quote=True)}">
        <div class="frame">
          <div class="image-comparison">
            {image("sample-original", t["photo"] + " — " + t["before"], responsive=True, eager=True)}
            <div class="comparison-effect">
              {image("slit-flare", t["photo"] + " — Slit Flare", responsive=True, target=True, eager=True)}
            </div>
            <input class="comparison-control" type="range" min="0" max="100" value="50" aria-label="{t["compare"]}" hidden>
            <span class="comparison-handle" aria-hidden="true">
              <span class="comparison-knob" hidden><svg width="19" height="12" viewBox="0 0 19 12" fill="currentColor"><path d="M7 2.5 3 6l4 3.5zm5 0L16 6l-4 3.5z"/></svg></span>
            </span>
          </div>
          <span class="tag" data-preview-label lang="en">Slit Flare</span>
        </div>
        <div class="landing-looks-meta">
          <p class="landing-look-name" data-preview-label aria-live="polite" lang="en">Slit Flare</p>
          <p class="note">{credit(camera=True)} · {t["demo"]}</p>
        </div>
        <div class="landing-strip" role="group" aria-label="{t["links"][0]}" data-preview-options hidden>
{full_tiles}
        </div>
        <noscript><p class="note">{link("guide/#styles", t["browse"])}</p></noscript>
      </div>
    </section>

    <section class="section" id="strength" aria-labelledby="strength-title">
      <h2 id="strength-title">{title}</h2>
      <div class="section-body">
        <p class="landing-text">{intro}</p>
        <div class="landing-framing" data-preview data-alt="{escape(t["photo"], quote=True)} — Slit Flare">
          <div class="landing-choices" role="group" aria-label="{title}" data-preview-options hidden>
            {options(forces, ("slit-weak", "slit-flare", "slit-strong"), 1, mono=True)}
          </div>
          <div class="landing-framing-views">
            <figure>
              <div class="frame">{image("slit-flare", t["photo"] + " — Slit Flare, " + forces[1], target=True)}</div>
              <figcaption><b>{jpeg}</b> · Slit Flare · <span data-preview-label aria-live="polite">{forces[1]}</span></figcaption>
            </figure>
            <figure>
              <div class="frame">{image("sample-original-small", t["photo"] + " — " + t["before"])}</div>
              <figcaption><b>{t["before"]}</b></figcaption>
            </figure>
          </div>
        </div>
        <p class="note">{t["preview"]}</p>
      </div>
    </section>

    <section class="section" aria-labelledby="details-title">
      <h2 id="details-title">{more}</h2>
      <div class="section-body">
        <div class="landing-details">
          <figure class="landing-detail" data-preview data-alt="{escape(t["photo"], quote=True)} — Slit Flare">
            <div class="frame">{image("slit-flare", t["photo"] + " — Slit Flare, " + sides[0], target=True)}<span class="tag" data-preview-label>{sides[0]}</span></div>
            <figcaption>
              <h3>{position}</h3>
              <p>{position_note}</p>
              <div class="landing-choices" role="group" aria-label="{position}" data-preview-options hidden>
                {options(sides, ("slit-flare", "slit-left", "slit-top", "slit-bottom"), 0)}
              </div>
              <p class="note">{credit()}</p>
            </figcaption>
          </figure>
          <figure class="landing-detail" data-preview data-alt="{escape(t["photo"], quote=True)} — Slit Flare">
            <div class="frame">{image("slit-flare", t["photo"] + " — Slit Flare, " + variation + " 1", target=True)}<span class="tag" data-preview-label>{variation} 1</span></div>
            <figcaption>
              <h3>{variation}</h3>
              <p>{variation_note}</p>
              <div class="landing-choices" role="group" aria-label="{variation}" data-preview-options hidden>
                {options(tuple(variation + " " + v for v in variants), ("slit-flare", "slit-variation-2", "slit-variation-3"), 0)}
              </div>
              <p class="note">{credit()}</p>
            </figcaption>
          </figure>
        </div>
      </div>
    </section>

    <section class="section" aria-labelledby="camera-title">
      <h2 id="camera-title">{camera}</h2>
      <div class="section-body">
        <div class="landing-screens">
          <figure>
            <div class="camera-diagram" aria-hidden="true">
              <p>{diagram} · ADJ → GRK</p>
              <div class="camera-diagram-tiles">
                <img src="{assets}0-module.png" width="40" height="40" alt="" loading="lazy">
                <img src="{assets}2-slit.png" width="40" height="40" alt="" loading="lazy">
                <img src="{assets}3-burnt.png" width="40" height="40" alt="" loading="lazy">
                <img src="{assets}4-band.png" width="40" height="40" alt="" loading="lazy">
              </div>
              <p>Light Leak</p>
            </div>
            <figcaption>{adj}</figcaption>
          </figure>
          <figure>
            <div class="camera-diagram" aria-hidden="true">
              <p>{diagram} · Fn</p>
              <p>{title} · {position} · {variation}</p>
              <p>Show Icon</p>
            </div>
            <figcaption>{fn} {link("guide/", t["links"][3])}</figcaption>
          </figure>
        </div>
      </div>
    </section>

    <section class="section" id="faq" aria-labelledby="faq-title">
      <h2 id="faq-title">{t["faq-title"]}</h2>
      <div class="section-body faq">
{faq}
      </div>
    </section>

    <section class="section" aria-labelledby="safe-title">
      <h2 id="safe-title">{safe}</h2>
      <div class="section-body">
        <ul class="landing-facts">
{fact_markup}
        </ul>
        <a class="download landing-install landing-safe-install" href="install/">{install}</a>
      </div>
    </section>

    <section class="section" id="original-project" aria-labelledby="original-project-title">
      <h2 id="original-project-title">{upstream_title}</h2>
      <div class="section-body">
        <p class="landing-text">{upstream_credit}</p>
      </div>
    </section>
  </main>"""


def replace(pattern, source, markup):
    result, count = re.subn(pattern, lambda _: markup, source, flags=re.MULTILINE | re.DOTALL)
    if count != 1:
        raise ValueError(f"expected one match, found {count}: {pattern}")
    return result


def sync(verify=False):
    stale = []
    for lang in COPY:
        for page in ("", "install", "guide"):
            path = ROOT / lang_path(lang, page) / "index.html"
            root = "../" * (len(path.relative_to(ROOT).parts) - 1) or "./"
            before = path.read_text(encoding="utf-8")
            source = replace(r"^  <header class=\"masthead\">.*?^  </header>", before, header(lang, page, root))
            source = replace(r"^  <footer class=\"footer\">.*?^  </footer>", source, footer(lang, root))
            source = re.sub(r"^  <!-- Google Tag Manager -->\n.*?^  <!-- End Google Tag Manager -->\n", "", source, flags=re.MULTILINE | re.DOTALL)
            source = re.sub(r"^  <!-- Google Tag Manager \(noscript\) -->\n.*?^  <!-- End Google Tag Manager \(noscript\) -->\n", "", source, flags=re.MULTILINE | re.DOTALL)
            charset = '  <meta charset="utf-8">'
            source = replace(f"^{charset}$", source, f"{charset}\n{GTM_HEAD}")
            source = replace(r"^<body[^>]*>", source, f'<body class="page">\n{GTM_BODY}')
            source = re.sub(r'^  <script defer src="[^"]+/site\.js"></script>\n', "", source, flags=re.MULTILINE)
            source = source.replace("</head>", f'  <script defer src="{root}assets/site.js"></script>\n</head>')
            alt = COPY[lang]["photo"] + " — Slit Flare"
            source = replace(r'^  <meta property="og:image:alt"[^>]+>', source, f'  <meta property="og:image:alt" content="{escape(alt, quote=True)}">')
            if not page:
                source = replace(r"^  <main id=\"main\".*?^  </main>", source, home_main(lang, root))
            if source != before:
                if verify:
                    stale.append(str(path.relative_to(ROOT)))
                else:
                    path.write_text(source, encoding="utf-8")
    if stale:
        raise SystemExit("Regenerate these pages: " + ", ".join(stale))


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--check", action="store_true")
    sync(parser.parse_args().check)
